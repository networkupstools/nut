//TODO: not in final
#include <malamute.h>
#include <neon/ne_xml.h>
#include "bestpower-mib.c"
/*
 *      HEADER FILE
 *
 */
#define DEFAULT_CAPACITY 16

#define LOOKUP "lookup"
#define SNMP "snmp"
#define ALARM "alarm"

#define INFO_LOOKUP_MAX_ATTRS 4
#define INFO_SNMP_MAX_ATTRS 16
#define INFO_ALARM_MAX_ATTRS 6

#define INFO_LOOKUP "lookup_info"
#define LOOKUP_OID "oid"
#define LOOKUP_VALUE "value"

#define INFO_SNMP "snmp_info"
#define SNMP_NAME "name"
#define SNMP_MULTIPLIER "multiplier"
#define SNMP_OID "oid"
#define SNMP_DEFAULT "default"
#define SNMP_LOOKUP "lookup"
#define SNMP_SETVAR "setvar"
#define SNMP_INFOFLAG_WRITABLE "writable"
#define SNMP_INFOFLAG_STRING "string"
#define SNMP_FLAG_STATIC "static"
#define SNMP_FLAG_ABSENT "absent"

#define INFO_ALARM "info_alarm"
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
char
*get_param_by_name (const char *name, const char **items)
{
    int iname;

    if (!items || !name) return NULL;
    iname = 0;
    while (items[iname]) {
        if (strcmp (items[iname],name) == 0) {
            return strdup(items[iname+1]);
        }
        iname += 2;
    }
    return NULL;
}

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
snmp_info_t *
info_snmp_new (const char *name, double multiplier, const char *oid, const char *dfl, info_lkp_t *lookup, int *setvar)
{
    snmp_info_t *self = (snmp_info_t*) malloc (sizeof (snmp_info_t));
    assert (self);
    memset (self, 0, sizeof (snmp_info_t));
    if(name)
      self->info_type = strdup (name);
    self->info_len = multiplier;
    if(oid)
      self->OID = strdup (oid);
    if(dfl)
      self->dfl = strdup (dfl);
    self->oid2info = lookup;
    self->setvar = setvar;
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

void
info_snmp_destroy (void **self_p)
{
    int i = 0;
    if (*self_p) {
        snmp_info_t *self = (snmp_info_t*) *self_p;
	printf("Destroying: %s ---> %f ---> %s---> %s\n",self->info_type, self->info_len, self->OID, self->dfl);
        if (self->info_type)
	{
            free ((char*)self->info_type);
            self->info_type = NULL;
        }
        if (self->OID)
	{
            free ((char*)self->OID);
            self->OID = NULL;
        }
        if (self->dfl)
	{
            free ((char*)self->dfl);
            self->dfl = NULL;
        }
        if (self->oid2info)
	{
	    while(!((self->oid2info[i].oid_value == 0) && (!self->oid2info[i].info_value))){
	      printf("Info_lkp_t-----------> %d",self->oid2info[i].oid_value);
	      if(self->oid2info[i].info_value){
		printf("  value---> %s\n",self->oid2info[i].info_value);
		free((void*)self->oid2info[i].info_value);
		self->oid2info[i].info_value = NULL;
	      }
	      i++;
	    }
            free ((info_lkp_t*)self->oid2info);
            self->oid2info = NULL;
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
  if(self)
    return (alist_t*)self->values[self->size-1];
  return NULL;
}

alist_t *alist_get_element_by_name(alist_t *self, char *name)
{
  int i;
  if(self)
    for(i = 0; i < self->size; i++)
      if(strcmp(((alist_t*)self->values[i])->name, name) == 0)
	return (alist_t*)self->values[i];
  return NULL;
}
//I splited because with the error control is going a grow a lot
void
alarm_info_node_handler(alist_t *list, const char **attrs)
{
    alist_t *element = alist_get_last_element(list);
    int i=0;
    char **arg = (char**) malloc ((INFO_ALARM_MAX_ATTRS + 1) * sizeof (void**));
    assert (arg);
    memset (arg, 0, (INFO_ALARM_MAX_ATTRS + 1) * sizeof(void**));
    while((attrs[i])&&(i < INFO_ALARM_MAX_ATTRS))
    {
      arg[i] = strdup(attrs[i]);
      i++;
    }

    if(arg[0])
      if(arg[3]){
	if(strcmp(arg[2], ALARM_OID) == 0)
	  alist_append(element, ((alarms_info_t *(*) (const char *, const char *, const char *)) element->new_element) (arg[1], arg[3], arg[5]));
	if(strcmp(arg[2], ALARM_STATUS) == 0)
	  alist_append(element, ((alarms_info_t *(*) (const char *, const char *, const char *)) element->new_element) (arg[1], NULL, arg[3]));
      }
    
    i = 0;
    while(arg[i])
    {
      free (arg[i]);
      i++;
    }
    free (arg);
}

void
lookup_info_node_handler(alist_t *list, const char **attrs)
{
    alist_t *element = alist_get_last_element(list);
    int i=0;
    char **arg = (char**) malloc ((INFO_LOOKUP_MAX_ATTRS + 1) * sizeof (void**));
    assert (arg);
    memset (arg, 0, (INFO_LOOKUP_MAX_ATTRS + 1) * sizeof(void**));
    
    while((attrs[i])&&(i < INFO_LOOKUP_MAX_ATTRS))
    {
      arg[i] = strdup(attrs[i]);
      i++;
    }

    if(arg[0])
	alist_append(element, ((info_lkp_t *(*) (int, const char *)) element->new_element) (atoi(arg[1]), arg[3]));
    
    i = 0;
    while(arg[i])
    {
      free (arg[i]);
      i++;
    }
    free (arg);
}

void
snmp_info_node_handler(alist_t *list, const char **attrs)
{
    //temporal
    int *x=0;
    //end tremporal
    info_lkp_t *lookup = NULL;
    alist_t *element = alist_get_last_element(list);
    int i=0;
    char **arg = (char**) malloc ((INFO_SNMP_MAX_ATTRS + 1) * sizeof (void**));
    assert (arg);
    memset (arg, 0, (INFO_SNMP_MAX_ATTRS + 1) * sizeof(void**));
    
    arg[0] = get_param_by_name(SNMP_NAME, attrs);
    arg[1] = get_param_by_name(SNMP_MULTIPLIER, attrs);
    arg[2] = get_param_by_name(SNMP_OID, attrs);
    arg[3] = get_param_by_name(SNMP_DEFAULT, attrs);
    arg[4] = get_param_by_name(SNMP_LOOKUP, attrs);
    arg[5] = get_param_by_name(SNMP_OID, attrs);
    arg[6] = get_param_by_name(SNMP_SETVAR, attrs);
    
    //flags
    /*arg[8] = get_param_by_name(SNMP_INFOFLAG_WRITABLE, attrs);
    arg[9] = get_param_by_name(SNMP_INFOFLAG_STRING, attrs);
    arg[10] = get_param_by_name(SNMP_FLAG_STATIC, attrs);
    arg[11] = get_param_by_name(SNMP_FLAG_ABSENT, attrs);*/
    
    if(arg[4]){
      alist_t *lkp = alist_get_element_by_name(list, arg[4]);
      lookup = (info_lkp_t*) malloc((lkp->size + 1) * sizeof(info_lkp_t));
      for(i = 0; i < lkp->size; i++){
	lookup[i].oid_value = ((info_lkp_t*) lkp->values[i])->oid_value;
	if(((info_lkp_t*) lkp->values[i])->info_value)
	  lookup[i].info_value = strdup(((info_lkp_t*) lkp->values[i])->info_value);
	else lookup[i].info_value = NULL;
      }
      lookup[i].oid_value = 0;
      lookup[i].info_value = NULL;
    }
    if(arg[0])
	alist_append(element, ((snmp_info_t *(*) (const char *, double, const char *, const char *, info_lkp_t *, int *)) element->new_element) (arg[0], atof(arg[1]), arg[2], arg[3], lookup, x));
    
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
  else if(strcmp(name,ALARM) == 0)
  {
    alist_append(list, alist_new(attrs[1], info_alarm_destroy, (void (*)(void)) info_alarm_new));
    printf(" %s   Its matched\n",attrs[1]);
  }
  else if(strcmp(name,SNMP) == 0)
  {
    alist_append(list, alist_new(attrs[1], info_snmp_destroy, (void (*)(void)) info_snmp_new));
    printf(" %s   Its matched\n",attrs[1]);
  }
  else if(strcmp(name,INFO_LOOKUP) == 0)
  {
    lookup_info_node_handler(list,attrs);
  }
  else if(strcmp(name,INFO_ALARM) == 0)
  {
    alarm_info_node_handler(list,attrs);
  }else if(strcmp(name,INFO_SNMP) == 0)
  {
    snmp_info_node_handler(list,attrs);
  }
  return 1;
}

int xml_end_cb(void *userdata, int state, const char *nspace, const char *name)
{
  if(!userdata)return ERR;
  if(strcmp(name,LOOKUP) == 0)
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
