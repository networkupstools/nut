//TODO: not in final
#include <malamute.h>

/*
 *      HEADER FILE
 *
 */
#define DEFAULT_CAPACITY 16

typedef struct {
	int oid_value;			/* OID value */
	const char *info_value;	/* INFO_* value */
} info_lkp_t;

typedef struct {
	void **values;
	int size;
	int capacity;
} alist_t;

typedef enum {
    ERR = -1,
    OK
} state_t;

// Create and initialize info_lkp_t
info_lkp_t *
    info_lkp_new (int oid, const char *value);

// Destroy and NULLify the reference to info_lkp_t
void
    info_lkp_destroy (void **self_p);

// Create new instance of alist
alist_t *
    alist_new ();

/*
 *
 *  C FILE
 *
 */

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

void
info_lkp_destroy (void **self_p)
{
    if (*self_p) {
        info_lkp_t *self =(info_lkp_t*) *self_p;

        if (self->info_value) {
            free ((char*)self->info_value);
            self->info_value = NULL;
        }
        free (self);
        
    }
}

alist_t *alist_new ()
{
  alist_t *self = (alist_t*) malloc (sizeof (alist_t));
  assert (self);
  memset (self, 0, sizeof(alist_t));
  self->size = 0;
  self->capacity = DEFAULT_CAPACITY;
  self->values = (void**) malloc (self->capacity * sizeof (void*));
  assert (self->values);
  memset (self->values, 0, self->capacity);
  return self;
}

void
alist_destroy (alist_t **self_p)
{
    if (*self_p)
    {
        alist_t *self = *self_p;
        for (int i = 0; i != self->size; i++)
            info_lkp_destroy ((void**)& self->values [self->size]);
        free (self->values);
        free (self);
    }
}


void alist_append(alist_t *self,void *element)
{
/*TODO Check when allocatd memory get full for reallocate more*/
  if(self->size<self->capacity){
    self->values[self->size] = element;
    self->size++;
  }
}

int main ()
{
    info_lkp_t * lkp = info_lkp_new (1, "one");
    alist_t * list = alist_new();
    alist_append(list,lkp);
    alist_destroy(&list);
    info_lkp_destroy((void**)&lkp);
    
}
