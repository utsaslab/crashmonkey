from __future__ import print_function

from keras.models import Model
from keras.layers import Input, Embedding, LSTM, Dense
import numpy as np

batch_size = 2  
# Batch size for training defines the number of samples that going to be propagated
# through the network in one pass (Forward+Backward).
# Smaller the batch_size: lesser memory requirements, faster training,
#                         less accurate gradient estimates

epochs = 100
# Number of epochs to train for.
# Epoch is defined as the number a forward and a backward pass of "all" training samples

latent_dim = 20
# Latent dimensionality of the encoding space.
# Corresponds to dimension of the internal state vectors

num_samples = 3  
# Number of training data samples to train on.

data_path = 'sample-bio-seq-dist.txt'
# Path to the data txt file on disk.

# Vectorize the data.
input_bio_sequences = []
target_distributions = []

input_vocabulary = set()
target_vocabulary = set()

data_lines = open(data_path).read().split('\n')
for line in data_lines[: min(num_samples, len(data_lines) - 1)]:
    input_bio_sequence, target_distribution = line.split('\t')
    input_string, output_string = line.split('\t')

    # We use "tab" as the "start sequence" character
    # for the targets, and "\n" as "end sequence" character.
    output_string = output_string.strip('[')
    output_string = output_string.strip(']')
    
    target_distribution = output_string.split(',')
    target_distribution = [float(data_value) for data_value in target_distribution]
    
    input_string = input_string.strip("[[")
    input_string = input_string.strip("]]")
    input_string = input_string.split("],[")
    input_bio_sequence = []
    for bio in input_string:
        bio = bio.split(',')
        bio = [float(bio_value) for bio_value in bio]
        input_bio_sequence.append(tuple(bio))

    target_distribution.insert(0,-2)
    target_distribution.append(2)

    input_bio_sequences.append(input_bio_sequence)
    target_distributions.append(target_distribution)

    # print("target_distribution : ", target_distribution)
    # print("input_bio_sequence : ", input_bio_sequence)

    # flag_exists = 0
    # for bio in input_bio_sequence:
    #     for vocab in input_vocabulary:
    #         if vocab == bio:
    #             flag_exists = 1
    #             break
    #     if flag_exists == 0:
    #         input_vocabulary.add(bio)
    #     flag_exists = 0

    for bio in input_bio_sequence:
        if bio not in input_vocabulary:
            input_vocabulary.add(bio)

    for probability in target_distribution:
        if probability not in target_vocabulary:
            target_vocabulary.add(probability)


input_vocabulary = sorted(list(input_vocabulary))
target_vocabulary = sorted(list(target_vocabulary))
# print("input_vocabulary : ", input_vocabulary)
# print("target_vocabulary : ", target_vocabulary)

num_encoder_tokens = len(input_vocabulary)
num_decoder_tokens = len(target_vocabulary)
max_encoder_seq_length = max([len(bio_sequence) for bio_sequence in input_bio_sequences])
max_decoder_seq_length = max([len(target) for target in target_distributions])

print('Number of samples:', len(input_bio_sequences))
print('Number of unique input tokens:', num_encoder_tokens)
print('Number of unique output tokens:', num_decoder_tokens)
print('Max sequence length for inputs:', max_encoder_seq_length)
print('Max sequence length for outputs:', max_decoder_seq_length)


input_token_index = dict(
    [(bio, i) for i, bio in enumerate(input_vocabulary)])
target_token_index = dict(
    [(bio, i) for i, bio in enumerate(target_vocabulary)])

# print('input_token_index : ', input_token_index)
# print('target_token_index : ', target_token_index)

# -----------------------------------------------------------------------------------------------------

encoder_input_data = np.zeros(
    (len(input_bio_sequences), max_encoder_seq_length, num_encoder_tokens),
    dtype='float32')
# print('encoder_input_data : ', encoder_input_data)

decoder_input_data = np.zeros(
    (len(input_bio_sequences), max_decoder_seq_length, num_decoder_tokens),
    dtype='float32')
# print('decoder_input_data : ', decoder_input_data)

decoder_target_data = np.zeros(
    (len(input_bio_sequences), max_decoder_seq_length, num_decoder_tokens),
    dtype='float32')
# print('decoder_target_data : ', decoder_target_data)

for i, (input_bio_sequence, target_distribution) in enumerate(zip(input_bio_sequences, target_distributions)):
    for t, bio in enumerate(input_bio_sequence):
        # print('input_token_index : ', input_token_index)
        # print('bio : ', bio)
        # print(input_token_index[bio])
        encoder_input_data[i, t, input_token_index[bio]] = 1.
    for t, bio in enumerate(target_distribution):
        # decoder_target_data is ahead of decoder_input_data by one timestep
        decoder_input_data[i, t, target_token_index[bio]] = 1.
        if t > 0:
            # decoder_target_data will be ahead by one timestep
            # and will not include the start character.
            decoder_target_data[i, t - 1, target_token_index[bio]] = 1.

# print('encoder_input_data : ', encoder_input_data)
# print('decoder_input_data : ', decoder_input_data)
# print('decoder_target_data : ', decoder_target_data)

# Define an input sequence and process it.
# encoder_inputs = Input(shape=(max_encoder_seq_length, num_encoder_tokens))
# Define an input sequence and process it.
encoder_inputs = Input(shape=(None, num_encoder_tokens))
encoder = LSTM(latent_dim, return_state=True)
encoder_outputs, state_h, state_c = encoder(encoder_inputs)
# We discard `encoder_outputs` and only keep the states.
encoder_states = [state_h, state_c]
# print('encoder_inputs_shape : ', np.shape(encoder_inputs))
# print('encoder_outputs', encoder_outputs)
# print('state_h : ', state_h)
# print('state_c : ', state_c)

# Set up the decoder, using `encoder_states` as initial state.
decoder_inputs = Input(shape=(None, num_decoder_tokens))
  # We set up our decoder to return full output sequences,
# and to return internal states as well. We don't use the
# return states in the training model, but we will use them in inference.
decoder_lstm = LSTM(latent_dim, return_sequences=True, return_state=True)
decoder_outputs, _, _ = decoder_lstm(decoder_inputs,
                                     initial_state=encoder_states)
decoder_dense = Dense(num_decoder_tokens, activation='softmax')
decoder_outputs = decoder_dense(decoder_outputs)

# Define the model that will turn
# `encoder_input_data` & `decoder_input_data` into `decoder_target_data`
model = Model([encoder_inputs, decoder_inputs], decoder_outputs)

# Run training
model.compile(optimizer='rmsprop', loss='categorical_crossentropy')
model.fit([encoder_input_data, decoder_input_data], decoder_target_data,
          batch_size=batch_size,
          epochs=epochs,
          validation_split=0.2)
# Save model
model.save('bios2probs.h5')

# Next: inference mode (sampling).
# Here's the drill:
# 1) encode input and retrieve initial decoder state
# 2) run one step of decoder with this initial state
# and a "start of sequence" token as target.
# Output will be the next target token
# 3) Repeat with the current target token and current states

# Define sampling models
encoder_model = Model(encoder_inputs, encoder_states)

decoder_state_input_h = Input(shape=(latent_dim,))
decoder_state_input_c = Input(shape=(latent_dim,))
decoder_states_inputs = [decoder_state_input_h, decoder_state_input_c]
decoder_outputs, state_h, state_c = decoder_lstm(
    decoder_inputs, initial_state=decoder_states_inputs)
decoder_states = [state_h, state_c]
decoder_outputs = decoder_dense(decoder_outputs)
decoder_model = Model(
    [decoder_inputs] + decoder_states_inputs,
    [decoder_outputs] + decoder_states)

# Reverse-lookup token index to decode sequences back to
# something readable.
reverse_input_char_index = dict(
    (i, bio) for bio, i in input_token_index.items())
reverse_target_char_index = dict(
    (i, bio) for bio, i in target_token_index.items())


def decode_sequence(input_seq):
    # Encode the input as state vectors.
    states_value = encoder_model.predict(input_seq)

    # Generate empty target sequence of length 1.
    target_seq = np.zeros((1, 1, num_decoder_tokens))
    # Populate the first character of target sequence with the start character.
    target_seq[0, 0, target_token_index[-2]] = 1.

    # Sampling loop for a batch of sequences
    # (to simplify, here we assume a batch of size 1).
    stop_condition = False
    decoded_sentence = [-2]
    while not stop_condition:
        output_tokens, h, c = decoder_model.predict(
            [target_seq] + states_value)

        # Sample a token
        sampled_token_index = np.argmax(output_tokens[0, -1, :])
        sampled_char = reverse_target_char_index[sampled_token_index]
        # print('decoded_sentence', decoded_sentence)
        # print('sampled_char', sampled_char)
        decoded_sentence.append(sampled_char)

        # Exit condition: either hit max length
        # or find stop character.
        if (sampled_char == 2 or
           len(decoded_sentence) > max_decoder_seq_length):
            stop_condition = True

        # Update the target sequence (of length 1).
        target_seq = np.zeros((1, 1, num_decoder_tokens))
        target_seq[0, 0, sampled_token_index] = 1.

        # Update states
        states_value = [h, c]

    return decoded_sentence[1:len(decoded_sentence)-1]


for seq_index in range(num_samples-1):
    # Take one sequence (part of the training test)
    # for trying out decoding.
    input_seq = encoder_input_data[seq_index: seq_index + 1]
    decoded_sentence = decode_sequence(input_seq)
    print('-')
    print('Input sentence:', input_bio_sequences[seq_index])
    print('Decoded sentence:', decoded_sentence)
