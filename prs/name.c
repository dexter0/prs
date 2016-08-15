/*
 *  Portable Runtime System (PRS)
 *  Copyright (C) 2016  Alexandre Tremblay
 *  
 *  This file is part of PRS.
 *  
 *  PRS is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *  
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 *  portableruntimesystem@gmail.com
 */

/**
 * \file
 * \brief
 *  This file contains the name resolver definitions.
 *
 *  The name resolver is a multi-writer multi-reader hash table that is typically used to map objects names to object
 *  IDs.
 */

#include <string.h>

#include <prs/pal/atomic.h>
#include <prs/pal/malloc.h>
#include <prs/error.h>
#include <prs/god.h>
#include <prs/name.h>
#include <prs/rtc.h>

typedef prs_uint32_t prs_name_entry_index_t;

struct prs_name_list_node {
    struct prs_name_list_node* PRS_ATOMIC
                                        next;
    PRS_ATOMIC prs_object_id_t          object_id;
};

struct prs_name_entry {
    struct prs_name_list_node* PRS_ATOMIC
                                        list;
};

struct prs_name {
    prs_size_t                          max_entries;
    prs_size_t                          string_offset;
    struct prs_name_entry*              entries;
};

static prs_name_entry_index_t prs_name_hash(const char* key)
{
    /* FNV-1a hash algorithm - http://isthe.com/chongo/tech/comp/fnv/ */
    const char* p = key;
    prs_uint32_t hash = 0x811C9DC5;
    while (*p) {
        hash = (*p++ ^ hash) * 0x01000193;
    }
    return hash;
}

static const char* prs_name_get_key(struct prs_name* name, void* object)
{
    return (const char*)object + name->string_offset;
}

static struct prs_name_entry* prs_name_get_entry(struct prs_name* name, const char* key)
{
    const prs_name_entry_index_t hash = prs_name_hash(key);
    const prs_name_entry_index_t entry_index = hash % name->max_entries;
    return &name->entries[entry_index];
}

/**
 * \brief
 *  Creates a name resolver.
 * \param params
 *  Name resolver parameters.
 */
struct prs_name* prs_name_create(struct prs_name_create_params* params)
{
    struct prs_name* name = prs_pal_malloc_zero(sizeof(*name));
    PRS_FATAL_WHEN(!name);

    name->max_entries = params->max_entries;
    name->string_offset = params->string_offset;
    name->entries = prs_pal_malloc_zero(sizeof(struct prs_name_entry) * name->max_entries);
    PRS_FATAL_WHEN(!name->entries);

    return name;
}

/**
 * \brief
 *  Destroys a name resolver.
 * \param name
 *  Name resolver to destroy.
 */
void prs_name_destroy(struct prs_name* name)
{
    for (prs_name_entry_index_t i = 0; i < name->max_entries; ++i) {
        struct prs_name_entry* entry = &name->entries[i];
        struct prs_name_list_node* PRS_ATOMIC* node_ptr = &entry->list;
        do {
            struct prs_name_list_node* node = prs_pal_atomic_load(node_ptr);
            if (node) {
                node_ptr = &node->next;
                prs_pal_free(node);
            } else {
                node_ptr = 0;
            }
        } while (node_ptr);
    }
    prs_pal_free(name->entries);
    prs_pal_free(name);
}

/**
 * \brief
 *  Allocates a hash table entry in the name resolver for the specified object.
 * \param name
 *  Name resolver to add to.
 * \param id
 *  ID of the object to add.
 * \return
 *  \ref PRS_NOT_FOUND if the object was not found using its \p id.
 *  \ref PRS_ALREADY_EXISTS if the object was already added.
 *  \ref PRS_OK if the object was successfully added.
 */
prs_result_t prs_name_alloc(struct prs_name* name, prs_object_id_t id)
{
    void* object = prs_god_lock(id);
    PRS_RTC_IF (!object) {
        return PRS_NOT_FOUND;
    }

    const char* str = prs_name_get_key(name, object);
    struct prs_name_entry* entry = prs_name_get_entry(name, str);

    struct prs_name_list_node* PRS_ATOMIC* node_ptr = &entry->list;
    struct prs_name_list_node* new_node = 0;

    prs_result_t result = PRS_OK;
    for (;;) {
        struct prs_name_list_node* node = prs_pal_atomic_load(node_ptr);
        if (node) {
            prs_object_id_t object_id = prs_pal_atomic_load(&node->object_id);
            PRS_RTC_IF (object_id == id) {
                result = PRS_ALREADY_EXISTS;
                break;
            }

            if (object_id == PRS_OBJECT_ID_INVALID) {
                const bool result = prs_pal_atomic_compare_exchange_strong(&node->object_id, &object_id, id);
                if (result) {
                    break;
                }
            } else {
                node_ptr = &node->next;
            }
        } else {
            if (!new_node) {
                new_node = prs_pal_malloc_zero(sizeof(*new_node));
                PRS_FATAL_WHEN(!new_node);
                prs_pal_atomic_store(&new_node->next, 0);
                prs_pal_atomic_store(&new_node->object_id, id);
            }

            const bool result = prs_pal_atomic_compare_exchange_strong(node_ptr, &node, new_node);
            if (result) {
                new_node = 0;
                break;
            }
        }
    }

    if (new_node) {
        prs_pal_free(new_node);
    }

    prs_god_unlock(id);

    return result;
}

/**
 * \brief
 *  Removes a hash table entry in the name resolver for the specified object.
 * \param name
 *  Name resolver to remove from.
 * \param id
 *  ID of the object to remove.
 * \return
 *  \ref PRS_NOT_FOUND if the object was not found using its \p id or in the hash table.
 *  \ref PRS_OK if the object was successfully removed.
 */
prs_result_t prs_name_free(struct prs_name* name, prs_object_id_t id)
{
    void* object = prs_god_lock(id);
    PRS_RTC_IF (!object) {
        return PRS_NOT_FOUND;
    }

    const char* str = prs_name_get_key(name, object);
    struct prs_name_entry* entry = prs_name_get_entry(name, str);

    struct prs_name_list_node* PRS_ATOMIC* node_ptr = &entry->list;
    struct prs_name_list_node* node;

    prs_result_t result = PRS_OK;

    for (;;) {
        node = prs_pal_atomic_load(node_ptr);
        if (node) {
            const prs_object_id_t object_id = prs_pal_atomic_load(&node->object_id);
            if (object_id == id) {
                prs_pal_atomic_store(&node->object_id, PRS_OBJECT_ID_INVALID);
                break;
            }
            node_ptr = &node->next;
        } else {
            result = PRS_NOT_FOUND;
            break;
        }
    }

    prs_god_unlock(id);

    return result;
}

static prs_object_id_t prs_name_find_internal(struct prs_name* name, const char* key, prs_bool_t keep_locked)
{
    struct prs_name_entry* entry = prs_name_get_entry(name, key);

    struct prs_name_list_node* PRS_ATOMIC* node_ptr = &entry->list;
    struct prs_name_list_node* node;

    for (;;) {
        node = prs_pal_atomic_load(node_ptr);
        if (node) {
            const prs_object_id_t object_id = prs_pal_atomic_load(&node->object_id);
            if (object_id != PRS_OBJECT_ID_INVALID) {
                void* object = prs_god_lock(object_id);
                if (object) {
                    const char* str = prs_name_get_key(name, object);
                    const prs_bool_t found = PRS_BOOL(!strcmp(str, key));
                    if (!found || !keep_locked) {
                        prs_god_unlock(object_id);
                    }
                    if (found) {
                        return object_id;
                    }
                }
            }
            node_ptr = &node->next;
        } else {
            return PRS_OBJECT_ID_INVALID;
        }
    }
}

/**
 * \brief
 *  Looks up the hash table for the specified key to find its matching object ID.
 * \param name
 *  Name resolver to search in.
 * \param key
 *  Key to search for.
 * \return
 *  The object ID that was found, or \ref PRS_OBJECT_ID_INVALID.
 */
prs_object_id_t prs_name_find(struct prs_name* name, const char* key)
{
    return prs_name_find_internal(name, key, PRS_FALSE);
}

/**
 * \brief
 *  Looks up the hash table for the specified key to find its matching object ID. Also, if an object ID is found,
 *  lock it.
 * \param name
 *  Name resolver to search in.
 * \param key
 *  Key to search for.
 * \return
 *  The object ID that was found, or \ref PRS_OBJECT_ID_INVALID.
 */
prs_object_id_t prs_name_find_and_lock(struct prs_name* name, const char* key)
{
    return prs_name_find_internal(name, key, PRS_TRUE);
}
