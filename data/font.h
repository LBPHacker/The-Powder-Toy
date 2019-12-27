#pragma once
#define FONT_H 12
#ifndef FONTEDITOR
extern unsigned char *font_data;
extern unsigned int *font_ptrs;
extern unsigned int (*font_ranges)[2];
#else
# error moooo lbphacker fix this
extern unsigned char *font_data;
extern unsigned short *font_ptrs;
extern unsigned int (*font_ranges)[2];
#endif
