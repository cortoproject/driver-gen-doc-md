/* This is a managed file. Do not delete this comment. */

#include <driver/gen/doc/md/md.h>
#include <corto/g/g.h>

typedef struct md_type {
    const char *name;
    corto_object meta_type;
    corto_ll types;
    bool exact_match;
    bool can_nest;
    bool have_scope;
} md_type;

typedef struct md_walkdata {
    g_generator g;
    md_type *types;
    g_file file;
    bool no_can_nest;
} md_walkdata;

static
char* md_type_id(
    corto_id buffer,
    corto_object o,
    md_walkdata *data)
{
    if (corto_parentof(o) == corto_lang_o) {
        return corto_fullpath(buffer, o);
    } else {
        return corto_path(buffer, g_getCurrent(data->g), o, ".");
    }
}

static
char *md_modifier_str(
    corto_modifierMask modifiers)
{
    char *str = NULL;
    corto_ptr_cast(
        corto_modifierMask_o, &modifiers, corto_string_o, &str);
    strlower(str);
    return str;
}

static
void md_generate_signature(
    corto_function function,
    bool parameters,
    md_walkdata *data)
{
    corto_id name;
    corto_sig_name(corto_idof(function), name);

    g_fileWrite(data->file, "%s %s(", corto_fullpath(NULL, corto_typeof(function)), name);
    if (!function->parameters.length) {
        g_fileWrite(data->file, ")");
    } else if (parameters) {
        g_fileWrite(data->file, "\n");
        g_fileIndent(data->file);
        int i;
        for (i = 0; i < function->parameters.length; i ++) {
            corto_parameter *p = &function->parameters.buffer[i];
            if (i) {
                g_fileWrite(data->file, ",\n");
            }
            if (p->inout != CORTO_IN) {
                if (p->inout == CORTO_OUT) {
                    g_fileWrite(data->file, "out ");
                } else {
                    g_fileWrite(data->file, "inout ");
                }
            }
            g_fileWrite(data->file, "%s %s",
                md_type_id(NULL, p->type, data),
                p->name);
        }
        g_fileWrite(data->file, ")");
        g_fileDedent(data->file);
    } else {
        g_fileWrite(data->file, "...)");
    }
    g_fileWrite(data->file, " %s\n", md_type_id(NULL, function->return_type, data));
}


static
void md_generate_interface_definition(
    corto_object o,
    md_walkdata *data)
{
    corto_interface type = o;
    g_fileWrite(data->file, "```corto\n");
    g_fileWrite(data->file, "%s %s",
        corto_fullpath(NULL, corto_typeof(o)),
        corto_idof(o));

    if (type->base) {
        g_fileWrite(data->file, ": %s",
            corto_path(NULL, g_getCurrent(data->g), type->base, "."));
    }

    if (corto_instanceof(corto_struct_o, type)) {
        corto_struct struct_type = corto_struct(type);
        if (struct_type->base_modifiers) {
            char *str = md_modifier_str(struct_type->base_modifiers);
            g_fileWrite(data->file, ", %s", str);
            free(str);
        }
    }

    g_fileWrite(data->file, " {\n");
    g_fileIndent(data->file);
    int i;
    for (i = 0; i < type->members.length; i ++) {
        corto_member member = type->members.buffer[i];
        g_fileWrite(data->file, "%s %s: %s",
            corto_fullpath(NULL, corto_typeof(member)),
            corto_idof(member),
            md_type_id(NULL, member->type, data));

        if (member->modifiers) {
            char *str = md_modifier_str(member->modifiers);
            g_fileWrite(data->file, ", %s", str);
            free(str);
        }
        g_fileWrite(data->file, "\n");
    }

    if (type->methods.length) {
        g_fileWrite(data->file, "\n");
        for (i = 0; i < type->methods.length; i ++) {
            md_generate_signature(type->methods.buffer[i], false, data);
        }
    }
    g_fileDedent(data->file);


    g_fileWrite(data->file, "}\n");
    g_fileWrite(data->file, "```\n");

}

static
void md_generate_initializer(
    corto_object o,
    md_walkdata *data)
{
    g_fileWrite(data->file, "### Declaration\n");
    corto_walk_opt s = corto_string_ser(
        CORTO_LOCAL | CORTO_READONLY | CORTO_PRIVATE | CORTO_HIDDEN, CORTO_NOT,
        CORTO_WALK_TRACE_ON_FAIL);
    s.aliasAction = CORTO_WALK_ALIAS_FOLLOW;
    s.optionalAction = CORTO_WALK_OPTIONAL_IF_SET;
    corto_string_ser_t sdata;
    memset(&sdata, 0, sizeof(corto_string_ser_t));
    corto_metawalk(&s, o, &sdata);
    char *str = corto_buffer_str(&sdata.buffer);
    g_fileWrite(data->file, "```corto\n");
    g_fileWrite(data->file, "%s obj = %s\n",
        corto_path(NULL, g_getCurrent(data->g), o, "."), str);
    g_fileWrite(data->file, "```\n");
}

static
int16_t md_generate_member(
    corto_walk_opt *opt,
    corto_value *info,
    void *userData)
{
    md_walkdata *data = userData;
    corto_member m = info->is.member.member;
    char *modifier_str = md_modifier_str(m->modifiers);
    g_fileWrite(data->file, "#### %s\n", corto_idof(m));
    g_fileWrite(data->file, "type | modifiers | unit | tags | default value\n");
    g_fileWrite(data->file, "-----|--------|------|------|--------------\n");
    g_fileWrite(data->file, "%s |", md_type_id(NULL, m->type, data));
    g_fileWrite(data->file, "`%s` |", modifier_str);
    if (m->unit) {
        g_fileWrite(data->file, "%s |", md_type_id(NULL, m->unit, data));
    } else {
        g_fileWrite(data->file, " - |");
    }

    corto_iter it = corto_ll_iter(m->tags);
    int count = 0;
    while (corto_iter_hasNext(&it)) {
        corto_object tag = corto_iter_next(&it);
        if (count) {
            g_fileWrite(data->file, ", ");
        }
        g_fileWrite(data->file, "%s |", md_type_id(NULL, tag, data));
        count ++;
    }
    if (!count) {
        g_fileWrite(data->file, " - |");
    }

    if (m->_default) {
        g_fileWrite(data->file, "`%s`", m->_default);
    } else {
        g_fileWrite(data->file, " - ");
    }

    g_fileWrite(data->file, "\n");

    g_fileWrite(data->file, "*No description*\n\n");
    free(modifier_str);
    return 0;
}

static
void md_generate_members(
    corto_interface o,
    md_walkdata *data)
{
    g_fileWrite(data->file, "### Members\n");

    if (o->base) {
        g_fileWrite(data->file, "#### super\n");
        g_fileWrite(data->file, "type | modifiers \n");
        g_fileWrite(data->file, "-----|--------\n");
        g_fileWrite(data->file, "%s |", md_type_id(NULL, o->base, data));
        if (corto_instanceof(corto_struct_o, o)) {
            char *modifier_str = md_modifier_str(corto_struct(o)->base_modifiers);
            g_fileWrite(data->file, "`%s`\n", modifier_str);
            free(modifier_str);
        } else {
            g_fileWrite(data->file, "`global`\n");
        }
    }

    corto_walk_opt s = CORTO_WALK_INIT;
    s.access = 0;
    s.accessKind = CORTO_NOT;
    s.metaprogram[CORTO_MEMBER] = md_generate_member;
    corto_metawalk(&s, o, data);
}

static
void md_generate_methods(
    corto_interface o,
    md_walkdata *data)
{
    int i, p;

    if (o->methods.length) {
        g_fileWrite(data->file, "### Methods\n");
    }

    for (i = 0; i < o->methods.length; i ++) {
        corto_function method = o->methods.buffer[i];
        if (method->overloaded) {
            /* Only use id with arguments if overloaded */
            g_fileWrite(data->file, "#### %s\n", corto_idof(method));
        } else {
            corto_id name;
            corto_sig_name(corto_idof(method), name);
            g_fileWrite(data->file, "#### %s\n", name);
        }

        g_fileWrite(data->file, "```corto\n");
        md_generate_signature(method, true, data);
        g_fileWrite(data->file, "```\n\n");

        for (p = 0; p < method->parameters.length; p ++) {
            corto_parameter *param = &method->parameters.buffer[p];
            g_fileWrite(data->file, "##### %s\n", param->name);
            g_fileWrite(data->file, "*No description*\n");
        }

        if (method->return_type && (
            method->return_type->reference ||
            !method->return_type->kind == CORTO_VOID))
        {
            g_fileWrite(data->file, "##### return\n");
            g_fileWrite(data->file, "*No description*\n");
        }
    }
}

static
int md_generate_interface(
    corto_object o,
    md_walkdata *data)
{
    md_generate_interface_definition(o, data);
    md_generate_initializer(o, data);
    md_generate_members(o, data);
    md_generate_methods(o, data);
    return 0;
}

static
void md_generate_enum_definition(
    corto_object o,
    bool is_bitmask,
    md_walkdata *data)
{
    corto_enum e = o;
    g_fileWrite(data->file, "```corto\n");
    g_fileWrite(data->file, "%s %s {\n",
        md_type_id(NULL, corto_typeof(o), data),
        corto_idof(o));

    int i;
    g_fileIndent(data->file);
    for (i = 0; i < e->constants.length; i ++) {
        g_fileWrite(data->file, "%s = ", corto_idof(e->constants.buffer[i]));
        if (!is_bitmask) {
            g_fileWrite(data->file, "%d\n", *(int32_t*)e->constants.buffer[i]);
        } else {
            g_fileWrite(data->file, "%x\n", *(uint32_t*)e->constants.buffer[i]);
        }
    }
    g_fileDedent(data->file);

    g_fileWrite(data->file, "}\n");
    g_fileWrite(data->file, "```\n");
}

static
void md_generate_enum_constants(
    corto_object o,
    md_walkdata *data)
{
    corto_enum e = o;

    g_fileWrite(data->file, "### Constants\n\n");
    g_fileWrite(data->file, "Constant | Description\n");
    g_fileWrite(data->file, "-------- | -----------\n");

    int i;
    for (i = 0; i < e->constants.length; i ++) {
        g_fileWrite(data->file, "%s | ",
            corto_idof(e->constants.buffer[i]));
        g_fileWrite(data->file, "*No description*\n");
    }
}

static
int md_generate_enum(
    corto_object o,
    md_walkdata *data)
{
    md_generate_enum_definition(o, false, data);
    md_generate_initializer(o, data);
    md_generate_enum_constants(o, data);
    return 0;
}

static
int md_generate_bitmask(
    corto_object o,
    md_walkdata *data)
{
    md_generate_enum_definition(o, true, data);
    md_generate_initializer(o, data);
    md_generate_enum_constants(o, data);
    return 0;
}

static
int md_generate(
    md_type *type,
    md_walkdata *data)
{
    if (corto_ll_count(type->types)) {
        if (type->can_nest) {
            g_fileWrite(data->file, "## %s\n", type->name);
        }

        corto_iter it = corto_ll_iter(type->types);
        while (corto_iter_hasNext(&it)) {
            corto_object o = corto_iter_next(&it);

            if (type->can_nest) {
                g_fileWrite(
                    data->file, "### %s %s\n",
                    corto_idof(corto_typeof(o)),
                    corto_path(NULL, g_getCurrent(data->g), o, "."));
            } else {
                g_fileWrite(
                    data->file, "## %s %s\n",
                    corto_idof(corto_typeof(o)),
                    corto_path(NULL, g_getCurrent(data->g), o, "."));
            }

            if (corto_instanceof(corto_interface_o, o)) {
                md_generate_interface(o, data);
            }

            if (corto_instanceof(corto_bitmask_o, o)) {
                md_generate_bitmask(o, data);
            } else
            if (corto_instanceof(corto_enum_o, o)) {
                md_generate_enum(o, data);
            }
        }
    }

    return 0;
}

static
void md_add_object(
    corto_object o,
    md_type *types,
    md_walkdata *data)
{
    int i;
    for (i = 0; types[i].name != NULL; i ++) {
        if (types[i].exact_match) {
            if (corto_typeof(o) != types[i].meta_type) {
                continue;
            }
        }
        if (corto_instanceof(types[i].meta_type, o)) {
            if (!data->no_can_nest || !types[i].can_nest) {
                if (types[i].types) {
                    corto_ll_append(types[i].types, o);
                }
            }
            break;
        }
    }
}

static
int md_walk_objects(
    corto_object o,
    void *userData)
{
    md_walkdata *data = userData;

    if (o != g_getCurrent(data->g)) {
        md_add_object(o, data->types, data);
    }

    return 1;
}

static
int md_walk_packages(
    corto_object o,
    void *userData)
{
    md_walkdata *data = userData;

    g_walkRecursive(data->g, md_walk_objects, data);

    return 1;
}

int genmain(g_generator g) {
    md_type types[] = {
        {"Packages", corto_package_o, corto_ll_new(), false, false, true},
        {"Classes", corto_class_o, corto_ll_new(), true, false, true},
        {"Structs", corto_struct_o, corto_ll_new(), true, false, true},
        {"Unions", corto_union_o, corto_ll_new(), false, false, true},
        {"Interfaces", corto_interface_o, corto_ll_new(), true, false, true},
        {"Collections", corto_collection_o, corto_ll_new(), false, false, true},
        {"Bitmasks", corto_bitmask_o, corto_ll_new(), false, false, false},
        {"Enumerations", corto_enum_o, corto_ll_new(), false, false, false},
        {"Primitives", corto_primitive_o, corto_ll_new(), false, false, false},
        {"Other types", corto_type_o, corto_ll_new(), false, false, false},
        {"Functions", corto_function_o, corto_ll_new(), true, true, false},
        {"Methods", corto_method_o, corto_ll_new(), false, true, false},
        {"Overrides", corto_override_o, corto_ll_new(), false, true, false},
        {"Overridables", corto_overridable_o, corto_ll_new(), false, true, false},
        {"Metaprocedures", corto_metaprocedure_o, corto_ll_new(), false, true, false},
        {"Other", corto_object_o, NULL, false, false},
        {NULL, NULL, NULL}
    };

    if (!g_getCurrent(g)) {
        corto_throw("need a scope to generate documentation for");
        goto error;
    }

    md_walkdata walkData = {.types = types, .g = g, .no_can_nest = true};

    /* Collect objects, one scope at a time */
    g_walkNoScope(g, md_walk_packages, &walkData);

    /* Open file for current scope */
    corto_id filename;
    sprintf(filename, "doc/model_%s.md", corto_idof(g_getCurrent(g)));
    walkData.file = g_fileOpen(g, filename);

    g_fileWrite(walkData.file, "# %s\n", corto_idof(g_getCurrent(g)));

    /* Generate code for type */
    int i;
    for (i = 0; types[i].name != NULL; i ++) {
        if (md_generate(&types[i], &walkData)) {
            goto error;
        }
    }

    return 0;
error:
    return -1;
}
