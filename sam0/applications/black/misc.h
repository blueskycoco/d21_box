#ifndef _MISC_H
#define _MISC_H
char *doit(char *text,char *item_str);
char *add_item_number(char *old,char *id, double text);
char *add_item_str(char *old,char *id,char *text);
#if CONSOLE_OUTPUT_ENABLED
void console_init(void);
#endif
#endif
