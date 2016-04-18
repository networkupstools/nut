#include "loader.h"
#include <neon/ne_xml.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
 * This is the original structure, with pair of values.
 */
typedef struct {
	int oid_value;			/* OID value */
	const char *info_value;	/* INFO_* value */
} info_lkp_t;

/*
 * This is a auxiliar structure, used for store the name and size of colection of pairs
 * is going a be used for have reference of the colectrion until the moment of fill
 * the definitive config struct "mib2nut_info_t". Each element represent one colection of pairs.
 */
typedef struct {
	info_lkp_t *values;
	int size;
	char *name;
} index_lkp_t;

/*
 * Signal for track the visited nodes in the XML
 */
typedef enum {
    XML_NONE = 1,
    XML_ROOT,
    XML_INFO,
    XML_LOOKUP,
    XML_SNMP
} xml_state_t;

typedef enum {
    ERR = -1,
    OK
} state_t;









/*Auxiliar array_list handler*/

/*
 * This struct represent the root, entry point for the collection, is auxiliar and used for keep references.
 * Is going a be only one element of this type
 */
typedef struct {
  index_lkp_t *array;
  int size;
}array_list;
/*
 * This function is useless, only for testing, print in the screen the elements of the memory tree.
 */
void print_array_list(array_list *arr){
  int x,y;
  for(x=0;x<arr->size;x++){
    printf("Section: %s size %d\n", arr->array[x].name,arr->array[x].size);/*Print name colection and size for each colection.*/
    for(y=0;y<arr->array[x].size;y++)
      printf("   %d -- %s \n",arr->array[x].values[y].oid_value, arr->array[x].values[y].info_value);/*Print elements in colection*/
  }
}
/*
 * For initialize the entry point "root" of lists.
 */
array_list* array_list_new(){
  array_list *arr = (array_list*) malloc(sizeof(array_list));
  if(!arr) return 0;
  arr->array = NULL;
  arr->size = 0;
  return arr;
}

/*
 * We call index to all the nodes witch references a collection of pairs
 * this function add a new pair in one collection, index is the collection, position is where is located and attrs are the data coming
 * from parser.
 */
int index_add(index_lkp_t *index, int pos, const char **attrs){
  
    index_lkp_t *info = &index[pos];
  
    if(info->values==NULL){/*Check if its first pair for crate new pair values array*/
      info->values=malloc(sizeof(info_lkp_t));
      if(info->values==NULL)return ERR;
      info->values->oid_value=atoi(attrs[1]);
      info->values->info_value=malloc(strlen(attrs[3]));
      strcpy(info->values->info_value,attrs[3]);/*Copy values form XML attrs*/
      info->size=1;/*Set counter a 1 element in values array of pairs*/
      
      /*printf("%d -- %s \n",data->array->values->oid_value, data->array->values->info_value);*/
      
    }else{/*If its a non empty colection we just add a pair*/
      info->values=realloc(info->values,(info->size+1)*sizeof(info_lkp_t));
      if(info->values==NULL)return ERR;
      info->values[info->size].oid_value=atoi(attrs[1]);
      info->values[info->size].info_value=malloc(strlen(attrs[3]));
      if(info->values[info->size].info_value==NULL)return ERR;
      strcpy(info->values[info->size].info_value,attrs[3]);
      info->size++;/*After set the values of the pair just increment the size counter of array of pairs*/
      
      /*printf("%d -- %s \n",data->array->values[data->array->size].oid_value, data->array->values[data->array->size].info_value);*/
    }
    return OK;
}

/*
 * We call array_list to the array of colections.
 * This function add a new collection index.
 */
int array_list_add(array_list *arr, index_lkp_t *index){
  
  if(arr->size==0){/*I case of the firs collection we should create index*/
    arr->array=malloc(sizeof(index_lkp_t));
  }else {
    arr->array=realloc(arr->array,(arr->size+1)*sizeof(index_lkp_t));/*Or add new element*/
  }
  
  if(arr->array==NULL)return ERR;
  arr->array[arr->size].values=index->values;/*Give pointer for array of pairs*/
  arr->array[arr->size].name=malloc(strlen(index->name));
  if(arr->array[arr->size].name==NULL)return ERR;
  strcpy(arr->array[arr->size].name,index->name);/*And set the name of the collection as is wrote in the XML*/
  arr->size++;/*Increment size of the array of collection in the root node*/
  return OK;
}









/*void* array_list_get_idx(array_list *al, int i);*/

/*End Auxiliar array_list handler*/

/*
 * Enter node call back from neon
 */
int xml_dict_start_cb(void *userdata, int parent,
                      const char *nspace, const char *name,
                      const char **attrs){
  array_list *data = (array_list*) userdata;
  if(strcmp(name,"info")==0){/*Indentifies witch type of node we are. In the case of "info" means that we sould setup a new colection*/
    index_lkp_t *index=malloc(sizeof(index_lkp_t));/*Index is a pointer referencing the new colection*/
    if(index==NULL)return ERR;
    index->name=malloc(strlen(attrs[1]));
    if(index->name==NULL)return ERR;
    strcpy(index->name,attrs[1]);/*Get the name from the XML attrs*/
    index->values=NULL;
    index->size=0;
    array_list_add(data,index);/*And call for set the new colection node in in the root structure*/
    
    /*printf("hola %s\n",name);*/
    return XML_INFO;
  }
  if(strcmp(name,"lookup")==0){/*Indentifies witch type of node we are. In the case of "lookup" jus we now that we have already colection and we sould add a new pair from XML*/
    index_add(data->array, data->size-1, attrs);
    return XML_LOOKUP;
  }
  
  return XML_NONE;
}

/*
 * Exit node call back from neon.
 * Useless now but wery useful for future, thats why didnt remove.
 */
int xml_end_cb(void *userdata, int state, const char *nspace, const char *name){
  /*array_list *data = (array_list*) userdata;*/
  
  if(strcmp(name,"info")==0){
    
    return 0;
  }
  if(strcmp(name,"lookup")==0){
    
    return 0;
  }
  return 0;
}


int main() {
  char buffer[1024];/*Just for read file*/
  int result = 0;/*for now when the parser finish*/
  array_list *data = array_list_new();/*The root node, unique and entry point in the tree*/
  ne_xml_parser *parser = ne_xml_create ();
  ne_xml_push_handler (parser, xml_dict_start_cb, NULL, xml_end_cb, data);
  
  FILE *f = fopen ("test.xml", "r");
    if (f) {
        while (!feof (f)) {/*read file and call parser until get the all XML*/
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
    print_array_list(data);
    return result;
}/*TODO Unallocate fuction, for free the memory is not impremented yet*/
