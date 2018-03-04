#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

typedef enum 
{
	SET_VAR_CHECK_VAL_OK = 0,
	SET_VAR_CHECK_VAL_VAR_NOT_SUPPORTED,
	SET_VAR_CHECK_VAL_READONLY,
	SET_VAR_CHECK_VAL_SET_FAILED,
	SET_VAR_CHECK_VAL_TOO_LONG,
	SET_VAR_CHECK_VAL_INVALID_VALUE
} set_var_check_val_t;

set_var_check_val_t set_var_check_val(upstype_t *ups, const char *var, const char *newval);
int do_set_var(upstype_t *ups, const char *var, const char *newval);

void net_set(nut_ctype_t *client, int numarg, const char **arg);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

