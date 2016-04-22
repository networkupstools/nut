//TODO: not in final
#include <malamute.h>
#include <neon/ne_xml.h>
#include "bestpower-mib.c"
/*
 *      HEADER FILE
 *
 */
#define DEFAULT_CAPACITY 16

#define INFO "info"
#define LOOKUP "lookup"
#define LOOKUP_OID "oid"
#define LOOKUP_VALUE "value"
#define ALARM "alarm"
#define ALARM_OID "oid"
#define ALARM_STATUS "status"
#define ALARM_ALARM "alarm"



typedef struct {
	void **values;
	int size;
	int capacity;
	char *name;
	void (*destroy)(void **self_p);
	void (*new_element)(void);
} alist_t;

typedef enum {
    ERR = -1,
    OK
} state_t;

// Create and initialize info_lkp_t
info_lkp_t *
    info_lkp_new (int oid, const char *value);

// Destroy and NULLify the reference to alist_t, list of collections
void
    info_lkp_destroy (void **self_p);

// Create new instance of alist with LOOKUP type, for storage a list of collections
alist_t *
    alist_new ();

/*
 *
 *  C FILE
 *
 */

//Create a lookup elemet
info_lkp_t *
info_lkp_new (int oid, const char *value)
{
    info_lkp_t *self = (info_lkp_t*) malloc (sizeof (info_lkp_t));
    assert (self);
    memset (self, 0, sizeof (info_lkp_t));
    self->oid_value = oid;
    if(value)
      self->info_value = strdup (value);
    return self;
}

//Create alarm element
alarms_info_t *
info_alarm_new (const char *oid, const char *status, const char *alarm)
{
    alarms_info_t *self = (alarms_info_t*) malloc (sizeof (alarms_info_t));
    assert (self);
    memset (self, 0, sizeof (alarms_info_t));
    if(oid)
      self->OID = strdup (oid);
    if(status)
      self->status_value = strdup (status);
    if(alarm)
      self->alarm_value = strdup (alarm);
    return self;
}

//Destroy full array of lookup elements
void
info_lkp_destroy (void **self_p)
{
    if (*self_p) {
        info_lkp_t *self = (info_lkp_t*) *self_p;
	printf("Destroying: %d ---> %s\n",self->oid_value, self->info_value);
        if (self->info_value)
	{
            free ((char*)self->info_value);
            self->info_value = NULL;
        }
        free (self);
	*self_p = NULL;
    }
}

//Destroy full array of alarm elements
void
info_alarm_destroy (void **self_p)
{
    if (*self_p) {
        alarms_info_t *self = (alarms_info_t*) *self_p;
	printf("Destroying: %s ---> %s ---> %s\n",self->OID, self->status_value, self->alarm_value);
        if (self->OID)
	{
            free ((char*)self->OID);
            self->OID = NULL;
        }
        if (self->status_value)
	{
            free ((char*)self->status_value);
            self->status_value = NULL;
        }
        if (self->alarm_value)
	{
            free ((char*)self->alarm_value);
            self->alarm_value = NULL;
        }
        free (self);
	*self_p = NULL;
    }
}

//New generic list element (can be the root element)
alist_t *alist_new (const char *name, void (*destroy)(void **self_p), void (*new_element)(void))
{
  alist_t *self = (alist_t*) malloc (sizeof (alist_t));
  assert (self);
  memset (self, 0, sizeof(alist_t));
  self->size = 0;
  self->capacity = DEFAULT_CAPACITY;
  self->values = (void**) malloc (self->capacity * sizeof (void*));
  assert (self->values);
  memset (self->values, 0, self->capacity);
  self->destroy = destroy;
  self->new_element = new_element;
  if(name)
    self->name = strdup(name);
  else 
    self->name = NULL;
  return self;
}

//Destroy full array of generic list elements
void
alist_destroy (alist_t **self_p)
{
    if (*self_p)
    {
        alist_t *self = *self_p;
	
	printf("N elements %d \n",self->size);
	if(self->name)printf("** Name collection: %s \n",self->name);
	
        for (;self->size>0; self->size--){
	  //This printf is only for show test result
	  printf("Destroying %d ---> %s\n",((info_lkp_t*) *(self->values))->oid_value, ((info_lkp_t*) *(self->values))->info_value);
	  self->destroy(& self->values [self->size-1]);
	}
	if(self->name)
	  free(self->name);
        free (self->values);
        free (self);
	*self_p = NULL;
    }
}

//Add a generic element at the end of the list
void alist_append(alist_t *self,void *element)
{
  if(self->size==self->capacity)
  {
    self->capacity+=DEFAULT_CAPACITY;
    self->values = (void**) realloc(self->values, self->capacity * sizeof(void*));
  }
    self->values[self->size] = element;
    self->size++;
}

//Return the last element of the list
alist_t *alist_get_last_element(alist_t *self)
{
    return (alist_t*)self->values[self->size-1];
}

//I splited because with the error control is going a grow a lot
void
info_node_handler(alist_t *list, const char **attrs)
{
    alist_t *element = alist_get_last_element(list);
    int i=0;
    int flag=0;
    char **arg = (char**) malloc (8 * sizeof (void**));
    assert (arg);
    memset (arg, 0, 8 * sizeof(void**));
    
    while((attrs[i])&&(i<8))
    {
      arg[i] = strdup(attrs[i]);
      i++;
    }

    if(arg[0])
    {
      if(strcmp(arg[0],ALARM_OID) == 0)
      {
	if(arg[2]){
	  if(strcmp(arg[2], ALARM_STATUS) == 0)
	  {
	    if(arg[4])alist_append(element, ((alarms_info_t *(*) (const char *, const char *, const char *)) element->new_element) (attrs[1], attrs[3], attrs[5]));
	    flag = 1;
	  }else if(strcmp(arg[2], ALARM_ALARM) == 0)
	  {
	    alist_append(element, ((alarms_info_t *(*) (const char *, const char *, const char *)) element->new_element) (attrs[1], NULL, attrs[3]));
	    flag = 1;
	  }
	}
      }if((strcmp(arg[0],LOOKUP_OID) == 0)&&(flag == 0))
      {
	alist_append(element, ((info_lkp_t *(*) (int, const char *)) element->new_element) (atoi(arg[1]), arg[3]));
      }
    }
    
    i = 0;
    while(arg[i])
    {
      free (arg[i]);
      i++;
    }
    free (arg);
}

int xml_dict_start_cb(void *userdata, int parent,
                      const char *nspace, const char *name,
                      const char **attrs)
{
  alist_t *list = (alist_t*) userdata;
  printf("Node --%s\n", name);
  if(!userdata)return ERR;
  if(strcmp(name,LOOKUP) == 0)
  {
    alist_append(list, alist_new(attrs[1], info_lkp_destroy,(void (*)(void)) info_lkp_new));
    printf(" %s   Its matched\n",attrs[1]);
  }
    if(strcmp(name,ALARM) == 0)
  {
    alist_append(list, alist_new(attrs[1], info_alarm_destroy, (void (*)(void)) info_alarm_new));
    printf(" %s   Its matched\n",attrs[1]);
  }
  if(strcmp(name,INFO) == 0)
  {
    info_node_handler(list,attrs);
  }
  return 1;
}

int xml_end_cb(void *userdata, int state, const char *nspace, const char *name)
{
  if(!userdata)return ERR;
  if(strcmp(name,"lookup") == 0)
  {
    
  }
  return OK;
  
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
    /* alist_t * list = alist_new(LOOKUP_LIST,NULL); */
    alist_t * list = alist_new(NULL,(void (*)(void **))alist_destroy, NULL);
    char buffer[1024];
    int result = 0;ne_xml_parser *parser = ne_xml_create ();
    ne_xml_push_handler (parser, xml_dict_start_cb, NULL, xml_end_cb, list);
    FILE *f = fopen ("test.xml", "r");
    if (f) {
        while (!feof (f)) {
            size_t len = fread(buffer, sizeof(char), sizeof(buffer), f);
            if (len == 0) {
                result = 1;
                break;
            } else {
                if ((result = ne_xml_parse (parser, buffer, len))) {
                    break;
                }
            }
        }
        if (!result) ne_xml_parse (parser, buffer, 0);
	/*printf("aqui %s", buffer);*/
        fclose (f);
    } else {
        result = 1;
    }
    ne_xml_destroy (parser);

    printf("Now checking what is in memory and destroying\n");
    alist_destroy(&list);
}
