#ifndef ORT_LANG_C_H
#define ORT_LANG_C_H

void	gen_c_header(const struct config *, const char *, int, int, int, int, int);
int	gen_c_source(const struct config *, int, int, int, int, const char *, const char *, const int *);

#endif /* !ORT_LANG_C_H */
