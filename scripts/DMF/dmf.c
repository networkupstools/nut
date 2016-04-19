//TODO: not in final
#include <malamute.h>


typedef struct {
	int oid_value;			/* OID value */
	const char *info_value;	/* INFO_* value */
} info_lkp_t;

typedef struct {
	info_lkp_t **values;
	int size;
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

/*  Use common API ad info_lkp_t
// step #1
index_lkp_new ();

// step #2
index_lkp_append (index_lkp_t *self, void *item);

// step #3
index_lkp_destroy (index_lkp_t **self_p);

// step #4
index_lkp_set_destructor (index_lkp_t *);
*/
int index_add(index_lkp_t *index, int pos, const char **attrs){
  
    if(!index)return ERR;
    
    index_lkp_t *info = &index[pos];
  
    if(info->values==NULL){/*Check if its first pair for crate new pair values array*/
      info->values=(info_lkp_t**)malloc(sizeof(info_lkp_t));
      assert(info->values);
      *(info->values)=NULL;
      info->values[0]=info_lkp_new(atoi(attrs[1]),attrs[3]);
      info->size=1;/*Set counter a 1 element in values array of pairs*/
     
    }else{/*If its a non empty colection we just add a pair*/
      info->values=(info_lkp_t**)realloc(info->values,(info->size+1)*sizeof(info_lkp_t));
      if(info->values==NULL)return ERR;
      info->values[info->size]=info_lkp_new(atoi(attrs[1]),attrs[3]);
      info->size++;/*After set the values of the pair just increment the size counter of array of pairs*/
      
    }
    return OK;
}
int index_del(index_lkp_t *index, int pos){
  if(!index)return ERR;
  printf("%s size: %d : Oid: %d Value: %s  \n",index->name, index->size, (*index->values)->oid_value, (*index->values)->info_value);
  index_lkp_t *info = &index[pos];
  info_lkp_destroy(info->values);
  free(info->values);
  return OK;
}

int main ()
{
    //info_lkp_t * lkp = info_lkp_new (1, "one");
    index_lkp_t *index=(index_lkp_t*)malloc(sizeof(index_lkp_t));
    assert(index);
    memset(index, 0, sizeof(index_lkp_t));
    const char *attrs[]={"Sensitivity","1","","OK",""};
    index->name=strdup(attrs[0]);
    assert(index);
    index_add(index,0,attrs);
    index_del(index,0);
    free(index->name);
    free(index);
    

}
