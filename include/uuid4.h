/**
 * Copyright (c) 2018 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef UUID4_H
#define UUID4_H

#define UUID4_VERSION "1.0.0"
#define UUID4_LEN 37

enum {
  UUID4_ESUCCESS =  0,
  UUID4_EFAILURE = -1
};

int  uuid4_init(void);
void uuid4_generate(char *dst);

#endif

