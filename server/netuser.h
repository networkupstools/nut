#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

void net_login(nut_ctype_t *client, int numarg, const char **arg);
void net_logout(nut_ctype_t *client, int numarg, const char **arg);
void net_master(nut_ctype_t *client, int numarg, const char **arg);
void net_username(nut_ctype_t *client, int numarg, const char **arg);
void net_password(nut_ctype_t *client, int numarg, const char **arg);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

