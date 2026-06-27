/*

    Amy - a chess playing program

    Copyright (c) 2002-2026, Thorsten Greiner
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.

*/

#include "amy.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "tree.h"
#include "yaml.h"

static void test_simple_dict(void) {
    const char *payload = "key: value\n";

    struct YamlNode *pResult = parse_yaml(payload);

    assert(pResult != NULL);

    assert(pResult->type == DICT);

    tree_node_t *pTree = (tree_node_t *)pResult->payload;

    size_t qwValueLen;
    struct YamlNode *pValue = (YamlNode *)lookup_value(pTree, "key", 4, &qwValueLen);

    assert(pValue != NULL);
    assert(qwValueLen == sizeof(struct YamlNode));

    assert(pValue->type == SCALAR);

    assert(!strcmp("value", (const char *)pValue->payload));

    free(pValue);

    free_yaml_node(pResult);
}

static void test_nested_dict(void) {
    const char *payload = "level1:\n"
                    "  level2: value";

    struct YamlNode *pResult = parse_yaml(payload);

    assert(pResult != NULL);
    assert(pResult->type == DICT);

    tree_node_t *pTree = (tree_node_t *)pResult->payload;

    size_t qwValueLen;
    struct YamlNode *pValue = (YamlNode *)lookup_value(pTree, "level1", 7, &qwValueLen);

    assert(pValue != NULL);
    assert(qwValueLen == sizeof(struct YamlNode));

    assert(pValue->type == DICT);

    tree_node_t *pTree2 = (tree_node_t *)pValue->payload;

    size_t qwValue2Len;
    struct YamlNode *pValue2 = (YamlNode *)lookup_value(pTree2, "level2", 7, &qwValue2Len);

    assert(pValue2 != NULL);

    assert(pValue2->type == SCALAR);
    assert(!strcmp("value", (const char *)pValue2->payload));

    free(pValue);
    free(pValue2);
    free_yaml_node(pResult);
}

static void test_multi_line_array(void) {
    const char *payload = "array:\n"
                    "- a\n"
                    "- b\n";

    struct YamlNode *pResult = parse_yaml(payload);

    assert(pResult != NULL);
    assert(pResult->type == DICT);

    tree_node_t *pTree = (tree_node_t *)pResult->payload;

    size_t qwValueLen;
    struct YamlNode *pValue = (YamlNode *)lookup_value(pTree, "array", 6, &qwValueLen);

    assert(pValue != NULL);
    assert(qwValueLen == sizeof(struct YamlNode));

    assert(pValue->type == LIST);
}

static void test_nested_multi_line_array(void) {
    const char *payload = "level1:\n"
                    "  array:\n"
                    "  - a\n"
                    "  - b\n"
                    "  scalar: x\n";

    struct YamlNode *pResult = parse_yaml(payload);

    assert(pResult != NULL);
    assert(pResult->type == DICT);

    tree_node_t *pTree = (tree_node_t *)pResult->payload;

    size_t qwValueLen;
    struct YamlNode *pValue = (YamlNode *)lookup_value(pTree, "level1", 7, &qwValueLen);

    assert(pValue != NULL);
    assert(qwValueLen == sizeof(struct YamlNode));

    assert(pValue->type == DICT);

    tree_node_t *nested = (tree_node_t *)pValue->payload;

    pValue = (YamlNode *)lookup_value(nested, "array", 6, &qwValueLen);

    assert(pValue != NULL);
    assert(qwValueLen == sizeof(struct YamlNode));

    assert(pValue->type == LIST);

    pValue = (YamlNode *)lookup_value(nested, "scalar", 7, &qwValueLen);

    assert(pValue != NULL);
    assert(qwValueLen == sizeof(struct YamlNode));

    assert(pValue->type == SCALAR);
}

static void test_nested_multi_line_array_2(void) {
    const char *payload = "level1:\n"
                    "  array:\n"
                    "  - a\n"
                    "  - b\n"
                    "scalar: x\n";

    struct YamlNode *pResult = parse_yaml(payload);

    assert(pResult != NULL);
    assert(pResult->type == DICT);

    tree_node_t *pTree = (tree_node_t *)pResult->payload;

    size_t qwValueLen;
    struct YamlNode *pValue = (YamlNode *)lookup_value(pTree, "level1", 7, &qwValueLen);

    assert(pValue != NULL);
    assert(qwValueLen == sizeof(struct YamlNode));

    assert(pValue->type == DICT);

    tree_node_t *nested = (tree_node_t *)pValue->payload;

    pValue = (YamlNode *)lookup_value(nested, "array", 6, &qwValueLen);

    assert(pValue != NULL);
    assert(qwValueLen == sizeof(struct YamlNode));

    assert(pValue->type == LIST);

    pValue = (YamlNode *)lookup_value(nested, "scalar", 7, &qwValueLen);

    assert(pValue == NULL);
}

static void test_get_as_string(void) {
    const char *payload = "key: value\n";

    struct YamlNode *pResult = parse_yaml(payload);
    assert(pResult != NULL);

    struct StringLookupResult lookup_result = get_as_string(pResult, "key");
    assert(lookup_result.result_code == OK);
    assert(lookup_result.result != NULL);
    assert(!strcmp(lookup_result.result, "value"));

    free_yaml_node(pResult);
}

static void test_get_as_string_not_found(void) {
    const char *payload = "key: value\n";

    struct YamlNode *pResult = parse_yaml(payload);
    assert(pResult != NULL);

    struct StringLookupResult lookupResult = get_as_string(pResult, "other_key");
    assert(lookupResult.result_code == NOT_FOUND);

    free_yaml_node(pResult);
}

static void test_get_as_string_nested(void) {
    const char *payload = "level1:\n"
                    "  level2: value";

    struct YamlNode *pResult = parse_yaml(payload);
    assert(pResult != NULL);

    struct StringLookupResult lookup_result =
        get_as_string(pResult, "level1.level2");
    assert(lookup_result.result_code == OK);
    assert(lookup_result.result != NULL);
    assert(!strcmp(lookup_result.result, "value"));

    free_yaml_node(pResult);
}

static void test_get_as_int(void) {
    const char *payload = "key: 500\n";

    struct YamlNode *pResult = parse_yaml(payload);
    assert(pResult != NULL);

    struct IntLookupResult lookup_result = get_as_int(pResult, "key");
    assert(lookup_result.result_code == OK);
    assert(lookup_result.result == 500);

    free_yaml_node(pResult);
}

static void test_get_as_int_format_error(void) {
    const char *payload = "key: x\n";

    struct YamlNode *pResult = parse_yaml(payload);
    assert(pResult != NULL);

    struct IntLookupResult lookup_result = get_as_int(pResult, "key");
    assert(lookup_result.result_code == FORMAT_ERROR);

    free_yaml_node(pResult);
}

static void test_get_as_list(void) {
    const char *payload = "key: [1, 0, 2]\n";

    struct YamlNode *pResult = parse_yaml(payload);
    assert(pResult != NULL);

    struct ListLookupResult lookup_result = get_as_list(pResult, "key");
    assert(lookup_result.result_code == OK);

    struct YamlListNode *pList = lookup_result.result;
    assert(pList != NULL);

    struct YamlListNode *next = pList->next;
    assert(next != NULL);

    next = next->next;
    assert(next != NULL);

    next = next->next;
    assert(next == NULL);

    free_yaml_node(pResult);
}

static void test_get_as_int_array(void) {
    const char *payload = "key: [1, 0, 2]\n";

    struct YamlNode *node = parse_yaml(payload);
    assert(node != NULL);

    int buf[3];

    struct IntArrayLookupResult lookup_result =
        get_as_int_array(node, "key", buf, 3);

    assert(lookup_result.result_code == OK);
    assert(lookup_result.elements_read == 3);
    assert(buf[0] == 1);
    assert(buf[1] == 0);
    assert(buf[2] == 2);

    free_yaml_node(node);
}

static void test_get_as_int_array_short(void) {
    const char *payload = "key: [1, -1, 2]\n";

    struct YamlNode *node = parse_yaml(payload);
    assert(node != NULL);

    int buf[2];

    struct IntArrayLookupResult lookup_result =
        get_as_int_array(node, "key", buf, 2);

    assert(lookup_result.result_code == OK);
    assert(lookup_result.elements_read == 2);
    assert(buf[0] == 1);
    assert(buf[1] == -1);

    free_yaml_node(node);
}

static void test_get_as_list_flow_style(void) {
    const char *payload = "key: [1,\n"
                    "  0, 2,\n"
                    "  3]\n";

    struct YamlNode *pResult = parse_yaml(payload);
    assert(pResult != NULL);

    struct ListLookupResult lookupResult = get_as_list(pResult, "key");
    assert(lookupResult.result_code == OK);

    struct YamlListNode *pList = lookupResult.result;
    assert(pList != NULL);

    struct YamlListNode *next = pList->next;
    assert(next != NULL);

    next = next->next;
    assert(next != NULL);

    next = next->next;
    assert(next != NULL);

    next = next->next;
    assert(next == NULL);

    free_yaml_node(pResult);
}

static void test_get_as_int_array_illegal_input(void) {
    const char *payload = "key: [1, a, 2]\n";

    struct YamlNode *node = parse_yaml(payload);
    assert(node != NULL);

    int buf[3];

    struct IntArrayLookupResult lookup_result =
        get_as_int_array(node, "key", buf, 3);

    assert(lookup_result.result_code == FORMAT_ERROR);

    free_yaml_node(node);
}

static void test_malformed_input(void) {
    const char *payload = "item1:\n  key: [1, a, 2\nitem2: scalar\n";
    struct YamlNode *node = parse_yaml(payload);
    assert(node == NULL);
}

static void test_list_trailing_comma(void) {
    const char *payload = "key: [1, 0,]\n";

    struct YamlNode *pResult = parse_yaml(payload);
    assert(pResult != NULL);

    struct ListLookupResult lookup_result = get_as_list(pResult, "key");
    assert(lookup_result.result_code == OK);

    struct YamlListNode *pList = lookup_result.result;
    assert(pList != NULL);

    struct YamlListNode *next = pList->next;
    assert(next != NULL);

    next = next->next;
    assert(next == NULL);

    free_yaml_node(pResult);
}

static void test_comments_and_empty_lines(void) {
    const char *payload = "key1: value1 # comment\n"
                    "\n\n"
                    "# top level comment\n"
                    "key2: value2\n"
                    "nested:\n"
                    "  key3: value3\n"
                    "\n\n"
                    "  key4: value4\n";

    struct YamlNode *pResult = parse_yaml(payload);
    assert(pResult != NULL);
    assert(pResult->type == DICT);

    struct StringLookupResult lookup_result = get_as_string(pResult, "key1");
    assert(lookup_result.result_code == OK);
    assert(lookup_result.result != NULL);
    assert(!strcmp(lookup_result.result, "value1"));

    lookup_result = get_as_string(pResult, "key2");
    assert(lookup_result.result_code == OK);
    assert(lookup_result.result != NULL);
    assert(!strcmp(lookup_result.result, "value2"));

    lookup_result = get_as_string(pResult, "nested.key3");
    assert(lookup_result.result_code == OK);
    assert(lookup_result.result != NULL);
    assert(!strcmp(lookup_result.result, "value3"));

    lookup_result = get_as_string(pResult, "nested.key4");
    assert(lookup_result.result_code == OK);
    assert(lookup_result.result != NULL);
    assert(!strcmp(lookup_result.result, "value4"));

    free_yaml_node(pResult);
}

void test_all_yaml(void) {
    test_simple_dict();
    test_nested_dict();
    test_multi_line_array();
    test_nested_multi_line_array();
    test_nested_multi_line_array_2();
    test_get_as_string();
    test_get_as_string_not_found();
    test_get_as_string_nested();
    test_get_as_int();
    test_get_as_int_format_error();
    test_get_as_list();
    test_get_as_list_flow_style();
    test_get_as_int_array();
    test_get_as_int_array_short();
    test_get_as_int_array_illegal_input();
    test_malformed_input();
    test_comments_and_empty_lines();
    test_list_trailing_comma();
}