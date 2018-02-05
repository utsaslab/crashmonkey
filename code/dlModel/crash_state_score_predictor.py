'''Trains a Bidirectional LSTM on the IMDB sentiment classification task.

Output after 4 epochs on CPU: ~0.8146
Time per epoch on CPU (Core i7): ~150s.
'''

from __future__ import print_function
import numpy as np

from keras.preprocessing import sequence
from keras.models import Sequential
from keras.layers import Dense, Dropout, Embedding, LSTM, Bidirectional, Activation
from keras.datasets import imdb

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


num_samples = 1648  
# Number of training data samples to train on.

X = []
Y = []
input_vocabulary = set()
output_vocabulary = set()
data_path = 'sample-crash-state-score.txt'
data_lines = open(data_path).read().split('\n')
for line in data_lines[: len(data_lines) - 2]:
	x, y = line.split('\t')
	x = x.strip(']]')
	x = x.strip('[[')
	x = x.split('],[')
	for i in range(0, len(x)):
		x[i] = x[i].split(',')

	for i in range(0, len(x)):
		if len(x[i]) != 7:
			continue
	for i in range(0, len(x)):
		for j in range(0, len(x[i])):
			x[i][j] = int(x[i][j])
	for i in range(0, len(x)):
		x[i] = tuple(x[i])
	print(x);
	print(type(x[0]));
	# sleep();
	y = int(y)
	X.append(x);
	Y.append(y)

	for x_val in x:
		print(type(x_val))
		if x_val not in input_vocabulary:
			input_vocabulary.add(x_val)
	if y not in output_vocabulary:
		output_vocabulary.add(y)

# X = np.array([np.array(xi) for xi in X])
# Y = np.array(Y);

# x_train = X[0:1300]
x_test = X[1300:]
# y_train = Y[0:1300]
y_test = Y[1300:]
# print(type(x_train))
# print(type(y_train))
# print(len(x_train), 'train sequences')
# print(len(x_test), 'test sequences')


input_vocabulary = sorted(list(input_vocabulary))
output_vocabulary = sorted(list(output_vocabulary))
# print("input_vocabulary : ", input_vocabulary)
# print("output_vocabulary : ", output_vocabulary)

num_encoder_tokens = len(input_vocabulary)
num_decoder_tokens = len(output_vocabulary)
max_encoder_seq_length = max([len(x_val) for x_val in X])
max_decoder_seq_length = 1

print('Number of samples:', len(X))
print('Number of unique input tokens:', num_encoder_tokens)
print('Number of unique output tokens:', num_decoder_tokens)
print('Max sequence length for inputs:', max_encoder_seq_length)
print('Max sequence length for outputs:', max_decoder_seq_length)


input_token_index = dict(
    [(x_val, i) for i, x_val in enumerate(input_vocabulary)])
target_token_index = dict(
    [(y_val, i) for i, y_val in enumerate(output_vocabulary)])

# print('input_token_index : ', input_token_index)
# print('target_token_index : ', target_token_index)

# -----------------------------------------------------------------------------------------------------

encoder_input_data = np.zeros(
    (len(X), max_encoder_seq_length, num_encoder_tokens),
    dtype='int')
# print('encoder_input_data : ', encoder_input_data)

# decoder_input_data = np.zeros(
#     (len(X), max_decoder_seq_length, num_decoder_tokens),
#     dtype='int')
# print('decoder_input_data : ', decoder_input_data)

encoder_target_data = np.zeros(
    (len(X), max_decoder_seq_length, num_decoder_tokens),
    dtype='int')
# print('decoder_target_data : ', decoder_target_data)

for i, (x_val, y_val) in enumerate(zip(X, Y)):
    for t, x in enumerate(x_val):
        encoder_input_data[i, t, input_token_index[x]] = 1
    if t > 0:
		encoder_target_data[i, t-1, target_token_index[y]] = 1



model = Sequential()
model.add(LSTM(128,input_shape=(max_encoder_seq_length, num_encoder_tokens),activation='sigmoid', inner_activation='hard_sigmoid', return_sequences=False))
# model.add(Activation('sigmoid'))
model.add(Dropout(0.2))
# model.add(TimeDistributedDense(11))
# model.add(Activation('softmax'))
model.compile(loss='categorical_crossentropy', optimizer='rmsprop')
model.fit(encoder_input_data, encoder_target_data, nb_epoch=30, batch_size=64)
scores = model.evaluate(x_test, y_test, verbose=0)
print("Accuracy: %.2f%%" % (scores[1]*100))