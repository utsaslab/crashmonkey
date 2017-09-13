//
// Created by Fábio Domingues on 19/07/17.
//

#ifndef CRASHMONKEY_UTILS_C_H
#define CRASHMONKEY_UTILS_C_H
#ifdef __cplusplus
extern "C"
{
#else
// get bool type definition
#include <linux/types.h>
#endif

#include "../disk_wrapper_ioctl.h"

bool c_is_async_write(struct disk_write_op_meta *m);
bool c_is_barrier_write(struct disk_write_op_meta *m);
bool c_is_meta(struct disk_write_op_meta *m);
bool c_has_write_flag(struct disk_write_op_meta *m);
bool c_has_flush_flag(struct disk_write_op_meta *m);
bool c_has_flush_seq_flag(struct disk_write_op_meta *m);
bool c_has_FUA_flag(struct disk_write_op_meta *m);
void c_set_flush_flag(struct disk_write_op_meta *m);
void c_set_flush_seq_flag(struct disk_write_op_meta *m);
void c_clear_flush_flag(struct disk_write_op_meta *m);
void c_clear_flush_seq_flag(struct disk_write_op_meta *m);

#ifdef __cplusplus
}
#endif
#endif //CRASHMONKEY_UTILS_C_H
