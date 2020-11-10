
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/* callback function from read_upsconf */
void do_upsconf_args(char *upsname, char *var, char *val);

/* open the ups.conf, parse it, and call back do_upsconf_args() */
void read_upsconf(void);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

