//TODO: not in final
#include <malamute.h>


typedef struct {
	int oid_value;			/* OID value */
	const char *info_value;	/* INFO_* value */
} info_lkp_t;


// Create and initialize info_lkp_t
info_lkp_t *
info_lkp_new (int oid, const char *value)
{
    info_lkp_t *self = (info_lkp_t*) malloc (sizeof (info_lkp_t));
    assert (self);
    memset (self, 0, sizeof (info_lkp_t));
    self->oid_value = oid;
    self->info_value = strdup (value);
    return self;
}

// Destroy and NULLify the reference to info_lkp_t
void
info_lkp_destroy (info_lkp_t **self_p)
{
    if (*self_p) {
        info_lkp_t *self = *self_p;

        if (self->info_value) {
            free ((char*)self->info_value);
            self->info_value = NULL;
        }
        free (self);
        *self_p = NULL;
    }
}

int main ()
{
    info_lkp_t * lkp = info_lkp_new (1, "one");
    assert (lkp);
    assert (lkp->oid_value == 1);
    info_lkp_destroy (&lkp);
    assert (!lkp);
    info_lkp_destroy (&lkp);

}
