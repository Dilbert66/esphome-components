//esphome:
//  includes:
//    - arduino_stacksize.h
//#esphome include file to set stack size from 8k to 16k

SET_LOOP_TASK_STACK_SIZE(16 * 1024);
