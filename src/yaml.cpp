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

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "safe_malloc.h"
#include "tree.h"
#include "yaml.h"

struct TokenizerState {
    const char *ptr;
    unsigned int indent_level;
    bool check_indent;
    bool flow_style;
    bool in_sequence;
};

void free_yaml_node(struct YamlNode *);
void free_list_node(struct YamlListNode *);
void free_tree_node(tree_node_t *tree);

static bool is_word_char(char c) { return isalnum(c) || c == '_' || c == '-'; }

static struct YamlToken token_from_type(YamlTokenType nType) {
    struct YamlToken result = {.type = nType, .text = NULL};
    return result;
}

struct YamlToken parse_word(struct TokenizerState *pState) {
    unsigned int dwLength = 0;
    unsigned int dwTrailingBlanks = 0;

    const char *begin = pState->ptr;
    while (is_word_char(*pState->ptr) || *pState->ptr == ' ') {
        if (*pState->ptr == ' ') {
            dwTrailingBlanks++;
        } else {
            dwTrailingBlanks = 0;
        }

        pState->ptr++;
        dwLength++;
    }

    dwLength -= dwTrailingBlanks;

    char *buffer = (char *)safe_malloc(dwLength + 1);

    memcpy(buffer, begin, dwLength);
    buffer[dwLength] = '\0';

    struct YamlToken result = {.type = TT_WORD, .text = buffer};
    return result;
}

static bool is_sequence_header(struct TokenizerState *pState) {
    return *pState->ptr == '-' && *(pState->ptr + 1) == ' ';
}

static struct YamlToken handle_indent(struct TokenizerState *pState,
                                  unsigned int dwIndent) {
    pState->check_indent = false;

    if (!pState->flow_style) {
        if (dwIndent > pState->indent_level) {
            pState->indent_level = dwIndent;
            return token_from_type(OPENING_BRACE);
        }

        if (dwIndent < pState->indent_level) {
            if (pState->in_sequence) {
                pState->in_sequence = false;
                pState->check_indent = true;
                return token_from_type(CLOSING_BRACKET);
            } else {
                pState->indent_level = dwIndent;
                return token_from_type(CLOSING_BRACE);
            }
        }

        if (is_sequence_header(pState)) {
            pState->ptr += 2;
            if (pState->in_sequence) {
                return token_from_type(COMMA);
            }
            pState->in_sequence = true;
            return token_from_type(OPENING_BRACKET);
        } else {
            if (pState->in_sequence) {
                pState->in_sequence = false;
                return token_from_type(CLOSING_BRACKET);
            }
        }
    }

    return token_from_type(CONTINUE);
}

/**
 * The lexer function - parses the input and returns the next token.
 */
static struct YamlToken next_token(struct TokenizerState *pState) {
    for (;;) {
        unsigned int dwIndent = 0;

        while (isblank(*pState->ptr)) {
            pState->ptr++;
            dwIndent++;
        }

        if (*pState->ptr == '\n') {
            pState->ptr++;
            pState->check_indent = true;
            continue;
        }

        if (*pState->ptr == '#') {
            do {
                pState->ptr++;
            } while (*pState->ptr != '\0' && *pState->ptr != '\n');
            if (*pState->ptr == '\n') {
                pState->check_indent = true;
                continue;
            }
        }

        if (pState->check_indent) {
            struct YamlToken result = handle_indent(pState, dwIndent);
            // printf("handle_indent() returned %d\n", result.type);
            if (result.type != CONTINUE) {
                return result;
            }
        }

        if (*pState->ptr == '\0') {
            if (pState->in_sequence) {
                pState->in_sequence = false;
                return token_from_type(CLOSING_BRACKET);
            }
            return token_from_type(END);
        }

        if (*pState->ptr == ':') {
            pState->ptr++;
            return token_from_type(COLON);
        }

        if (*pState->ptr == '[') {
            pState->ptr++;
            pState->flow_style = true;
            return token_from_type(OPENING_BRACKET);
        }

        if (*pState->ptr == ']') {
            pState->ptr++;
            pState->flow_style = false;
            return token_from_type(CLOSING_BRACKET);
        }

        if (*pState->ptr == ',') {
            pState->ptr++;
            return token_from_type(COMMA);
        }

        if (is_word_char(*pState->ptr)) {
            return parse_word(pState);
        }

        return token_from_type(UNKNOWN);
    }
}

struct YamlListNode *parse_list(struct TokenizerState *pState) {
    struct YamlListNode *pResult = NULL;
    struct YamlListNode *pLastNode = NULL;

    struct YamlToken token = next_token(pState);

    for (;;) {
        if (token.type == TT_WORD) {
            struct YamlNode *pValue =
                (struct YamlNode *)safe_malloc(sizeof(struct YamlNode));

            pValue->type = SCALAR;
            pValue->payload = token.text;
            // printf("Parsed list element: %s\n", token.text);

            struct YamlListNode *pNextNode =
                (struct YamlListNode *)safe_malloc(sizeof(struct YamlListNode));

            pNextNode->value = pValue;
            pNextNode->next = NULL;

            if (pResult == NULL) {
                pResult = pNextNode;
                pLastNode = pNextNode;
            } else {
                pLastNode->next = pNextNode;
                pLastNode = pNextNode;
            }

            token = next_token(pState);
            if (token.type == CLOSING_BRACKET) {
                // printf("List done.\n");
                return pResult;
            } else if (token.type == COMMA) {
                token = next_token(pState);
                continue;
            }
        } else if (token.type == CLOSING_BRACKET) {
            // printf("List done.\n");
            return pResult;
        }
        // printf("parse_list: Unexpected token %d!\n", token.type);
        free_list_node(pResult);
        return NULL;
    }
}

struct YamlNode *parse_dict(struct TokenizerState *pState) {
    tree_node_t *pResultDict = NULL;

    for (;;) {
        struct YamlToken token = next_token(pState);
        if (token.type == TT_WORD) {
            struct YamlToken expected_colon = next_token(pState);
            if (expected_colon.type != COLON) {
                free(token.text);
                printf("Expected ':', got token %d!\n", expected_colon.type);
                return NULL;
            }
            struct YamlToken expected_value = next_token(pState);
            if (expected_value.type == TT_WORD) {
                struct YamlNode node = {.type = SCALAR,
                                    .payload = expected_value.text};
                pResultDict =
                    add_node(pResultDict, token.text, strlen(token.text) + 1,
                             &node, sizeof(struct YamlNode));
            } else if (expected_value.type == OPENING_BRACKET) {
                struct YamlListNode *pListNode = parse_list(pState);
                if (pListNode == NULL) {
                    free(token.text);
                    free_tree_node(pResultDict);
                    // printf("Failed to parse list!\n");
                    return NULL;
                }
                struct YamlNode node = {.type = LIST, .payload = pListNode};
                pResultDict =
                    add_node(pResultDict, token.text, strlen(token.text) + 1,
                             &node, sizeof(struct YamlNode));
            } else if (expected_value.type == OPENING_BRACE) {
                struct YamlNode *pDictNode = parse_dict(pState);
                if (pDictNode == NULL) {
                    free(token.text);
                    free_tree_node(pResultDict);
                    // printf("Failed to parse dict!\n");
                    return NULL;
                }
                pResultDict =
                    add_node(pResultDict, token.text, strlen(token.text) + 1,
                             pDictNode, sizeof(struct YamlNode));
                free(pDictNode);
            } else {
                // printf("Unexpected token %d!\n", expected_value.type);
                free(token.text);
                free_tree_node(pResultDict);
                return NULL;
            }

            free(token.text);
        } else {
            break;
        }
    }

    // printf("Finished parsing dict.\n");

    struct YamlNode *pResult =
        (struct YamlNode *)safe_malloc(sizeof(struct YamlNode));

    pResult->type = DICT;
    pResult->payload = pResultDict;

    return pResult;
}

struct YamlNode *parse_yaml(const char *pszText) {
    struct TokenizerState state = {.ptr = pszText,
                                   .indent_level = 0,
                                   .check_indent = true,
                                   .flow_style = false,
                                   .in_sequence = false};
    return parse_dict(&state);
}

struct YamlNode *get_node(struct YamlNode *node, const char *path) {
    // Make a copy of path because strtok will clobber it
    char *path_buffer = (char *)safe_malloc(strlen(path) + 1);
    memcpy(path_buffer, path, strlen(path) + 1);

    char *pX = path_buffer;
    struct YamlNode current_node = *node;

    for (;;) {
        char *path_element = strtok(pX, ".");
        pX = NULL;

        if (path_element == NULL)
            break;

        if (current_node.type != DICT) {
            free(path_buffer);
            return NULL;
        }

        tree_node_t *pTree = (tree_node_t *)current_node.payload;
        size_t qwValueLen;
        struct YamlNode *pValue = (struct YamlNode *)lookup_value(
            pTree, path_element, strlen(path_element) + 1, &qwValueLen);

        if (pValue == NULL) {
            free(path_buffer);
            return NULL;
        }

        current_node = *pValue;
        free(pValue);
    }
    free(path_buffer);

    struct YamlNode *pResult =
        (struct YamlNode *)safe_malloc(sizeof(struct YamlNode));
    memcpy(pResult, &current_node, sizeof(struct YamlNode));

    return pResult;
}

/**
 * Lookup the string value under path. If successful (i.e. the node
 * exists and is a scalar) returns .result_code = OK and a copy of
 * the string value in .result. The caller is responsible for freeing
 * the string value with free().
 *
 * Returns .result_code = NOT_FOUND if the node identified by path does
 * not exist.
 *
 * Returns .result_code = TYPE_ERROR if the node identified by path is
 * not a scalar.
 */
struct StringLookupResult get_as_string(struct YamlNode *node, const char *path) {
    struct YamlNode *pTarget = get_node(node, path);

    if (pTarget == NULL) {
        struct StringLookupResult lookup_result = {.result_code = NOT_FOUND,
                                                   .result = NULL};
        return lookup_result;
    }

    char *pszResult = _strdup((const char *)pTarget->payload);
    int nType = pTarget->type;
    free(pTarget);

    if (nType != SCALAR) {
        struct StringLookupResult lookup_result = {.result_code = TYPE_ERROR,
                                                   .result = NULL};
        return lookup_result;
    }

    struct StringLookupResult lookup_result = {.result_code = OK,
                                              .result = pszResult};
    return lookup_result;
}

struct IntLookupResult get_as_int(struct YamlNode *node, const char *path) {
    struct YamlNode *pTarget = get_node(node, path);

    if (pTarget == NULL) {
        struct IntLookupResult lookup_result = {.result_code = NOT_FOUND,
                                                .result = 0};
        return lookup_result;
    }

    char *pszResult = (char *)pTarget->payload;
    int nType = pTarget->type;
    free(pTarget);

    if (nType != SCALAR) {
        struct IntLookupResult lookup_result = {.result_code = TYPE_ERROR,
                                                .result = 0};
        return lookup_result;
    }

    char *pszEnd;
    long nValue = strtol(pszResult, &pszEnd, 10);
    if (*pszEnd != '\0') { // Illegal character in the string
        struct IntLookupResult lookup_result = {.result_code = FORMAT_ERROR,
                                                .result = 0};
        return lookup_result;
    }
    if (nValue > INT_MAX || nValue < INT_MIN) { // overflow
        struct IntLookupResult lookup_result = {.result_code = FORMAT_ERROR,
                                                .result = 0};
        return lookup_result;
    }

    struct IntLookupResult lookup_result = {.result_code = OK,
                                            .result = (int)nValue};
    return lookup_result;
}

struct ListLookupResult get_as_list(struct YamlNode *node, const char *path) {
    struct YamlNode *pTarget = get_node(node, path);

    if (pTarget == NULL) {
        struct ListLookupResult lookup_result = {.result_code = NOT_FOUND,
                                                 .result = NULL};
        return lookup_result;
    }

    struct YamlListNode *pResult = (struct YamlListNode *)pTarget->payload;
    int nType = pTarget->type;
    free(pTarget);

    if (nType != LIST) {
        struct ListLookupResult lookup_result = {.result_code = TYPE_ERROR,
                                                 .result = NULL};
        return lookup_result;
    }

    struct ListLookupResult lookup_result = {.result_code = OK,
                                             .result = pResult};
    return lookup_result;
}

struct IntArrayLookupResult get_as_int_array(struct YamlNode *node, const char *path,
                                             int *buffer, int count) {
    struct YamlNode *pTarget = get_node(node, path);

    if (pTarget == NULL) {
        struct IntArrayLookupResult lookup_result = {.result_code = NOT_FOUND,
                                                     .elements_read = 0};
        return lookup_result;
    }

    struct YamlListNode *pListNode = (struct YamlListNode *)pTarget->payload;
    int nType = pTarget->type;
    free(pTarget);

    if (nType != LIST) {
        struct IntArrayLookupResult lookup_result = {.result_code = TYPE_ERROR,
                                                     .elements_read = 0};
        return lookup_result;
    }

    int nIndex = 0;
    for (; nIndex < count; nIndex++) {
        if (pListNode == NULL)
            break;
        struct YamlNode *pElem = pListNode->value;
        if (pElem->type != SCALAR) {
            struct IntArrayLookupResult lookup_result = {
                .result_code = TYPE_ERROR, .elements_read = 0};
            return lookup_result;
        }
        char *pszEnd;
        long nValue = strtol((const char *)pElem->payload, &pszEnd, 10);
        if (*pszEnd != '\0') { // Illegal character in the string
            struct IntArrayLookupResult lookup_result = {
                .result_code = FORMAT_ERROR, .elements_read = 0};
            return lookup_result;
        }
        if (nValue > INT_MAX || nValue < INT_MIN) { // overflow
            struct IntArrayLookupResult lookup_result = {
                .result_code = FORMAT_ERROR, .elements_read = 0};
            return lookup_result;
        }
        buffer[nIndex] = (int)nValue;
        pListNode = pListNode->next;
    }

    struct IntArrayLookupResult lookup_result = {
        .result_code = OK, .elements_read = (unsigned int)nIndex};
    return lookup_result;
}

void free_yaml_node(struct YamlNode *);

void free_list_node(struct YamlListNode *pListNode) {
    if (pListNode == NULL)
        return;
    free_yaml_node(pListNode->value);
    struct YamlListNode *next = pListNode->next;
    free(pListNode);
    free_list_node(next);
}

void free_tree_node(tree_node_t *pTree) {
    if (pTree == NULL) {
        return;
    }
    free_tree_node(pTree->left_child);
    free_tree_node(pTree->right_child);

    free_yaml_node((struct YamlNode *)pTree->value_data);
    free(pTree->key_data);

    free(pTree);
}

void free_yaml_node(struct YamlNode *node) {
    if (node->type == DICT) {
        tree_node_t *pTree = (tree_node_t *)node->payload;
        free_tree_node(pTree);
    } else if (node->type == SCALAR) {
        free((char *)node->payload);
    } else if (node->type == LIST) {
        struct YamlListNode *pList = (struct YamlListNode *)node->payload;
        free_list_node(pList);
    } else {
        printf("Unknown node type: %d\n", node->type);
    }
    free(node);
}