//TODO: not in final
#include <malamute.h>


typedef struct {
	int oid_value;			/* OID value */
	const char *info_value;	/* INFO_* value */
} info_lkp_t;

typedef struct {
	void **values;
	int size;
	int capacity;
	char *name;
} index_lkp_t;

typedef enum {
    ERR = -1,
    OK
} state_t;

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
void index_lkp_append(index_lkp_t *self,void *element){
/*  if(self->values==NULL){
    self->values=(void**)malloc(sizeof(void*));
    assert(self->values);
    self->values[self->size]=NULL;
  }else{
    self->values=realloc(self->values,self->size*sizeof());
  }*/
/*TODO Check when allocatd memory get full for reallocate more*/
  if(self->size<self->capacity){
  self->values[self->size]=element;
  self->size++;
  }
}

// Destroy and NULLify the reference to info_lkp_t
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
        *self_p = NULL;
    }
}

void index_lkp_delete_allvalues(index_lkp_t *self){
  do{
    info_lkp_destroy((void**)&self->values[self->size]);
    self->size--;
  }while(self->size>0);
  free(self->values);
}

/*  Use common API ad info_lkp_t*/
// step #1
index_lkp_t *index_lkp_new (const char *name){
  index_lkp_t *index=(index_lkp_t*)malloc(sizeof(index_lkp_t));
  assert(index);
  memset(index, 0, sizeof(index_lkp_t));
  index->name=strdup(name);
  index->size=0;
  index->values=(void**)malloc(16*sizeof(void*));
  assert(index->values);
  index->capacity=16;
  index->values[index->size]=NULL;
  return index;
}

// step #2
//index_lkp_append (index_lkp_t *self, void *item);

// step #3
//index_lkp_destroy (index_lkp_t **self_p);

// step #4
//index_lkp_set_destructor (index_lkp_t *);


int main ()
{
    //info_lkp_t * lkp = info_lkp_new (1, "one");
    const char *attrs[]={"Sensitivity","1","","OK",""};
    index_lkp_t *index=index_lkp_new(attrs[0]);
    info_lkp_t *element=info_lkp_new(atoi(attrs[1]),attrs[3]);
    index_lkp_append(index,element);
    
    index_lkp_delete_allvalues(index);
    //index_add(index,0,attrs);
    //index_del(index,0);
    free(index->name);
    
    free(index);
    

}
