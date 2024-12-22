/*!The Treasure Box Library
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Copyright (C) 2009-present, TBOOX Open Source Group.
 *
 * @author      ruki
 * @file        context.h
 * @ingroup     platform
 *
 */

/* modified by guo
 *
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *tb_context_ref_t;
typedef void *tb_cpointer_t;

// the context-from type
typedef struct __tb_context_from_t
{
    // the from-context
    tb_context_ref_t    context;

    // the passed user private data
    tb_cpointer_t       priv;

}tb_context_from_t;

/*! the context entry function type
 *
 * @param from          the from-context
 */
typedef void       (*tb_context_func_t)(tb_context_from_t from);

/* //////////////////////////////////////////////////////////////////////////////////////
 * interfaces
 */

/*! make context from the given the stack space and the callback function
 * Must jump to caller before func over, otherwise app will exit !!!
 *
 * @param stackdata     the stack data
 * @param stacksize     the stack size
 * @param func          the entry function
 *
 * @return              the context pointer
 */
extern tb_context_ref_t        tb_context_make(void* stackdata, int stacksize, tb_context_func_t func);

/*! jump to the given context
 *
 * @param context       the to-context
 * @param priv          the passed user private data
 *
 * @return              the from-context
 */
extern tb_context_from_t       tb_context_jump(tb_context_ref_t context, tb_cpointer_t priv);

#ifdef __cplusplus
}
#endif
