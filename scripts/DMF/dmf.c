//TODO: not in final
#include <malamute.h>
#include "bestpower-mib.c"
/*
 *      HEADER FILE
 *
 */
#define DEFAULT_CAPACITY 16

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
    info_lkp_destroy (info_lkp_t **self_p);

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
info_lkp_destroy (info_lkp_t **self_p)
{
    if (*self_p) {
        info_lkp_t *self =(info_lkp_t*) *self_p;

        if (self->info_value) {
            free ((char*)self->info_value);
            self->info_value = NULL;
        }
        free (self);
	*self_p = NULL;
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
	printf("N elements %d \n",self->size);
        for (;self->size>0; self->size--){
	  //This printf is only for show test result
	  printf("Destroying %d ---> %s\n",((info_lkp_t*) *(self->values))->oid_value, ((info_lkp_t*) *(self->values))->info_value);
            info_lkp_destroy ((info_lkp_t**)& self->values [self->size-1]);
	}
        free (self->values);
        free (self);
	*self_p = NULL;
    }
}


void alist_append(alist_t *self,void *element)
{
  if(self->size==self->capacity){
    self->capacity+=DEFAULT_CAPACITY;
    self->values = (void**) realloc(self->values, self->capacity * sizeof(void*));
  }
    self->values[self->size] = element;
    self->size++;
}

int main ()
{
    // info_lkp_t new/destroy test case
    info_lkp_t *info = info_lkp_new (1, "one");
    assert (info);
    assert (info->oid_value == 1);
    info_lkp_destroy (&info);
    assert (!info);
    info_lkp_destroy (&info);
    assert (!info);
    // alist new/destroy test case
    
    int i;
    alist_t * list = alist_new();
    for(i = 0; i<30; i++)//Exeded initial size for force realloc
      alist_append(list,info_lkp_new (1, "one"));
    alist_destroy(&list);
}
