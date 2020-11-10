#ifndef NUT_DESC_H_SEEN
#define NUT_DESC_H_SEEN 1

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

void desc_load(void);
void desc_free(void);
const char *desc_get_cmd(const char *name);
const char *desc_get_var(const char *name);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* NUT_DESC_H_SEEN */
