// x3_grammar.cpp — Boost.Spirit X3 grammar for Meld
//
// This is the authoritative grammar. The hand-written TokenParser/Lexer
// are being phased out in favour of these X3 rules.
#include "meld/parser/x3_grammar.hpp"
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/include/at_c.hpp>
#include <iostream>
#include <set>

namespace meld::parser::grammar {

namespace x3 = boost::spirit::x3;
namespace fusion = boost::fusion;
using namespace ast;

// ============================================================================
// Skipper
// ============================================================================

const skipper_type skipper = "skipper";
auto const line_comment  = x3::lit("//") >> *(x3::char_ - x3::eol) >> (x3::eol | x3::eoi);
auto const block_comment = x3::lit("/*") >> *(x3::char_ - "*/") >> "*/";
auto const skipper_def   = x3::ascii::space | line_comment | block_comment;
BOOST_SPIRIT_DEFINE(skipper)

// ============================================================================
// Identifiers (with kebab-case support)
// ============================================================================

auto const ident_char  = x3::char_("a-zA-Z0-9_");
auto const ident_start = x3::char_("a-zA-Z_");

// Kebab-case: hyphens allowed mid-identifier when followed by alnum/underscore.
// Cannot start or end with hyphen.
auto const kebab_cont = x3::char_('-') >> &ident_char;
auto const ident_body = ident_char | kebab_cont;

auto const kw_fnc   = x3::lexeme[ x3::lit("fnc")       >> !ident_body ];
auto const kw_val   = x3::lexeme[ x3::lit("val")       >> !ident_body ];
auto const kw_var   = x3::lexeme[ x3::lit("var")       >> !ident_body ];
auto const kw_rtn   = x3::lexeme[ x3::lit("rtn")       >> !ident_body ];
auto const kw_true  = x3::lexeme[ x3::lit("true")      >> !ident_body ];
auto const kw_false = x3::lexeme[ x3::lit("false")     >> !ident_body ];
auto const kw_typealias = x3::lexeme[ x3::lit("typealias") >> !ident_body ];
auto const kw_newtype   = x3::lexeme[ x3::lit("newtype")   >> !ident_body ];
auto const kw_type      = x3::lexeme[ x3::lit("type")      >> !ident_body ];
auto const kw_imp       = x3::lexeme[ x3::lit("imp")       >> !ident_body ];
auto const kw_namespace = x3::lexeme[ x3::lit("namespace") >> !ident_body ];
auto const kw_where     = x3::lexeme[ x3::lit("where")     >> !ident_body ];

static const std::set<std::string> reserved_words = {
    // True parser-level keywords
    "fnc", "val", "var", "rtn", "imp", "opr",
    "true", "false",
    // Banned keywords (produce errors in fallback parser)
    "import", "from", "as", "for", "while", "switch", "case",
    "break", "continue", "async", "await", "return", "function", "void"
};

struct make_identifier {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& raw = x3::_attr(ctx);
        std::string name(raw.begin(), raw.end());
        if (reserved_words.count(name)) {
            x3::_pass(ctx) = false;
            return;
        }
        x3::_val(ctx).name = std::move(name);
    }
};

auto const ast_ident = x3::rule<class aid_tag, identifier>{"identifier"} =
    x3::lexeme[ x3::raw[ ident_start >> *ident_body ] ][make_identifier{}];

// ============================================================================
// Semantic action functors
// ============================================================================

struct set_int_val {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        x3::_val(ctx).value = x3::_attr(ctx);
    }
};

struct set_bool_true_val {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        x3::_val(ctx).value = true;
    }
};

struct set_bool_false_val {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        x3::_val(ctx).value = false;
    }
};

struct set_string_val {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& range = x3::_attr(ctx);
        std::string raw(range.begin(), range.end());
        std::string result;
        result.reserve(raw.size());
        for (size_t i = 0; i < raw.size(); ++i) {
            if (raw[i] == '\\' && i + 1 < raw.size()) {
                switch (raw[++i]) {
                    case 'n':  result += '\n'; break;
                    case 't':  result += '\t'; break;
                    case 'r':  result += '\r'; break;
                    case '\\': result += '\\'; break;
                    case '"':  result += '"';  break;
                    case '0':  result += '\0'; break;
                    default:   result += '\\'; result += raw[i]; break;
                }
            } else {
                result += raw[i];
            }
        }
        x3::_val(ctx).value = result;
        x3::_val(ctx).has_interpolation = false;
        x3::_val(ctx).is_multiline = false;
    }
};

struct set_multiline_string_val {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& range = x3::_attr(ctx);
        x3::_val(ctx).value = std::string(range.begin(), range.end());
        x3::_val(ctx).has_interpolation = false;
        x3::_val(ctx).is_multiline = true;
    }
};

struct set_float_val {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        x3::_val(ctx).value = x3::_attr(ctx);
        x3::_val(ctx).suffix = "";
    }
};

struct make_unary_neg {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        unary_operation op;
        op.op = "-";
        op.operand = x3::_attr(ctx);
        x3::_val(ctx) = expression(op);
    }
};

struct make_unary_not {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        unary_operation op;
        op.op = "!";
        op.operand = x3::_attr(ctx);
        x3::_val(ctx) = expression(op);
    }
};

struct fold_char_binop {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        expression result = fusion::at_c<0>(attr);
        auto& rest = fusion::at_c<1>(attr);
        for (size_t i = 0; i < rest.size(); ++i) {
            binary_operation bin;
            char op_char = fusion::at_c<0>(rest[i]);
            bin.op = std::string(1, op_char);
            bin.left = result;
            bin.right = static_cast<const expression&>(fusion::at_c<1>(rest[i]));
            result = expression(bin);
        }
        x3::_val(ctx) = result;
    }
};

struct fold_string_binop {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        expression result = fusion::at_c<0>(attr);
        auto& rest = fusion::at_c<1>(attr);
        for (size_t i = 0; i < rest.size(); ++i) {
            binary_operation bin;
            bin.op = fusion::at_c<0>(rest[i]);
            bin.left = result;
            bin.right = static_cast<const expression&>(fusion::at_c<1>(rest[i]));
            result = expression(bin);
        }
        x3::_val(ctx) = result;
    }
};

struct fold_calls {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        expression result = fusion::at_c<0>(attr);
        auto& call_groups = fusion::at_c<1>(attr);
        for (size_t i = 0; i < call_groups.size(); ++i) {
            auto* id_ptr = boost::get<identifier>(&result);
            if (id_ptr) {
                function_call call;
                call.function_name = *id_ptr;
                auto& maybe_args = call_groups[i];
                if (maybe_args) {
                    for (size_t j = 0; j < maybe_args->size(); ++j) {
                        call.arguments.push_back((*maybe_args)[j]);
                    }
                }
                result = expression(call);
            }
        }
        x3::_val(ctx) = result;
    }
};

struct set_return {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& maybe_expr = x3::_attr(ctx);
        if (maybe_expr) {
            x3::_val(ctx).has_expression = true;
            x3::_val(ctx).expr = *maybe_expr;
        }
    }
};

struct set_val_decl {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).name = fusion::at_c<0>(attr);
        auto& mt = fusion::at_c<1>(attr);
        if (mt) { x3::_val(ctx).has_type_annotation = true; x3::_val(ctx).type_ann = *mt; }
        x3::_val(ctx).value = fusion::at_c<2>(attr);
    }
};

struct set_var_decl {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).name = fusion::at_c<0>(attr);
        auto& mt = fusion::at_c<1>(attr);
        if (mt) { x3::_val(ctx).has_type_annotation = true; x3::_val(ctx).type_ann = *mt; }
        x3::_val(ctx).value = fusion::at_c<2>(attr);
    }
};

struct set_func_param {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).name = fusion::at_c<0>(attr);
        x3::_val(ctx).type = fusion::at_c<1>(attr);
    }
};

struct set_func_param_mut {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).is_mutable = true;
        x3::_val(ctx).name = fusion::at_c<0>(attr);
        x3::_val(ctx).type = fusion::at_c<1>(attr);
    }
};

struct set_block {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& stmts = x3::_attr(ctx);
        for (auto& s : stmts) {
            x3::_val(ctx).statements.push_back(std::move(s));
        }
    }
};

struct set_func_def {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).name = fusion::at_c<0>(attr);
        auto& params = fusion::at_c<1>(attr);
        if (params) x3::_val(ctx).parameters = *params;
        auto& ret = fusion::at_c<2>(attr);
        if (ret) { x3::_val(ctx).has_return_type = true; x3::_val(ctx).return_type = *ret; }
        x3::_val(ctx).body = fusion::at_c<3>(attr);
    }
};

struct set_func_def_mut {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).is_mutating = true;
        x3::_val(ctx).name = fusion::at_c<0>(attr);
        auto& params = fusion::at_c<1>(attr);
        if (params) x3::_val(ctx).parameters = *params;
        auto& ret = fusion::at_c<2>(attr);
        if (ret) { x3::_val(ctx).has_return_type = true; x3::_val(ctx).return_type = *ret; }
        x3::_val(ctx).body = fusion::at_c<3>(attr);
    }
};

// ============================================================================
// Forward-declared rules
// ============================================================================

x3::rule<class expr_tag,      expression>          const expression_rule = "expression";
x3::rule<class toplevel_tag,  expression>          const toplevel_item   = "toplevel";
x3::rule<class lor_tag,       expression>          const logical_or_expr = "logical_or";
x3::rule<class land_tag,      expression>          const logical_and_expr = "logical_and";
x3::rule<class cmp_tag,       expression>          const comparison_expr = "comparison";
x3::rule<class add_tag,       expression>          const additive_expr   = "additive";
x3::rule<class mul_tag,       expression>          const mult_expr       = "multiplicative";
x3::rule<class unary_tag,     expression>          const unary_expr      = "unary";
x3::rule<class primary_tag,   expression>          const primary_expr    = "primary";
x3::rule<class stmt_tag,      expression>          const statement_rule  = "statement";
x3::rule<class block_tag,     block_expression>    const block_rule      = "block";
x3::rule<class func_tag,      function_definition>     const func_def        = "function_definition";
x3::rule<class ta_tag,        type_annotation>         const type_ann        = "type_annotation";
x3::rule<class ns_tag,        namespace_declaration>   const namespace_def   = "namespace_declaration";
x3::rule<class lambda_tag,    lambda_expression>       const lambda_rule     = "lambda";
x3::rule<class ext_tag,       extension_block>         const extension_def   = "extension_block";
x3::rule<class handle_tag,    handle_expression>       const handle_rule     = "handle_expression";
x3::rule<class anonimp_tag,  handle_expression>       const anon_impl_rule  = "anon_impl";
x3::rule<class intanimp_tag, handle_expression>      const intersect_impl  = "intersection_anon_impl";
x3::rule<class old_tag,      old_expression>          const old_rule        = "old_expression";
x3::rule<class tdestr_tag,   tuple_destructuring>     const tuple_destr_rule = "tuple_destructuring";

// ============================================================================
// Literals
// ============================================================================

auto const integer_lit = x3::rule<class int_tag, integer_literal>{} =
    x3::lexeme[ x3::int64 >> x3::attr(std::string{}) ];

auto const bool_lit = x3::rule<class bool_tag, boolean_literal>{} =
    (kw_true >> x3::attr(true)) | (kw_false >> x3::attr(false));

// Float literal: digits with decimal point, must have digits on at least one side.
// Parsed BEFORE integer to handle "3.14" correctly.
// Float must contain a decimal point to distinguish from integer
auto const float_lit = x3::rule<class flt_tag, float_literal>{} =
    x3::lexeme[ (&(*x3::digit >> '.') >> x3::double_) >> !ident_body ][set_float_val{}];

// String: double-quoted with escape processing (static, no interpolation)
auto const string_lit = x3::rule<class str_tag, string_literal>{} =
    x3::lexeme['"' >> x3::raw[ *(('\\' >> x3::char_) | (x3::char_ - '"')) ] >> '"'][set_string_val{}];

// Multiline string: triple-quoted (static, no interpolation)
auto const multiline_string_lit = x3::rule<class mstr_tag, string_literal>{} =
    x3::lexeme[x3::lit("\"\"\"") >> x3::raw[ *(x3::char_ - "\"\"\"") ] >> "\"\"\""][set_multiline_string_val{}];

// Template string: backtick (evaluated, with interpolation)
struct set_template_string_val {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& range = x3::_attr(ctx);
        x3::_val(ctx).value = std::string(range.begin(), range.end());
        x3::_val(ctx).has_interpolation = true;
        x3::_val(ctx).is_multiline = false;
        x3::_val(ctx).is_template = true;
    }
};

struct set_template_multiline_val {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& range = x3::_attr(ctx);
        x3::_val(ctx).value = std::string(range.begin(), range.end());
        x3::_val(ctx).has_interpolation = true;
        x3::_val(ctx).is_multiline = true;
        x3::_val(ctx).is_template = true;
    }
};

// Template multiline: ```...``` (must try before single backtick)
auto const template_multiline_lit = x3::rule<class tml_tag, string_literal>{} =
    x3::lexeme[x3::lit("```") >> x3::raw[ *(x3::char_ - "```") ] >> "```"][set_template_multiline_val{}];

// Template single-line: `...`
auto const template_string_lit = x3::rule<class tsl_tag, string_literal>{} =
    x3::lexeme['`' >> x3::raw[ *(('\\' >> x3::char_) | (x3::char_ - '`')) ] >> '`'][set_template_string_val{}];

// ============================================================================
// Type annotations (full: generics, union, intersection, nullable)
// ============================================================================

struct set_atomic_type {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).type_name = fusion::at_c<0>(attr);
        x3::_val(ctx).is_nullable = fusion::at_c<1>(attr);
        auto& args = fusion::at_c<2>(attr);
        if (args) {
            x3::_val(ctx).has_type_arguments = true;
            for (auto& a : *args) {
                x3::_val(ctx).type_arguments.push_back(a);
            }
        }
    }
};

// Generic type argument with optional val/var qualifier: val T, var U, or just T
struct set_qualified_type_arg {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        auto& maybe_qual = fusion::at_c<0>(attr);
        x3::_val(ctx) = fusion::at_c<1>(attr);
        if (maybe_qual) {
            if (*maybe_qual == "val")
                x3::_val(ctx).mutability_qualifier = type_annotation::MutabilityQualifier::VAL;
            else if (*maybe_qual == "var")
                x3::_val(ctx).mutability_qualifier = type_annotation::MutabilityQualifier::VAR;
        }
    }
};

auto const mut_qualifier = x3::rule<class mq_tag, std::string>{} =
    x3::lexeme[ x3::raw[ (x3::lit("val") | x3::lit("var")) >> !ident_body ] ];

auto const qualified_type_arg = x3::rule<class qta_tag, type_annotation>{} =
    (-mut_qualifier >> type_ann)[set_qualified_type_arg{}];

// Atomic type: Name?[val T, var U] — no union/intersection at this level
// Unit type: () — the empty tuple type
// Parse "()" and produce a type_annotation with name "()"
auto const unit_type_ann = x3::rule<class uta_tag, type_annotation>{"unit_type"} =
    x3::lit("(") >> x3::lit(")") >> x3::attr(identifier{"()"}) >> x3::attr(false);

// Function type: fnc(Type, Type) -> RetType or fnc(name: Type, name: Type) -> RetType
// Named params are accepted but names are discarded (only types matter for the type signature)
auto const fnc_type_params = kw_fnc >> '(' >> -(
    (x3::omit[-(x3::lexeme[ident_start >> *ident_body] >> ':')] >> type_ann) % ','
) >> ')' >> -("->" >> type_ann);

struct set_fnc_type_ann {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).type_name.name = "fnc";
        auto& params = fusion::at_c<0>(attr);
        if (params) {
            x3::_val(ctx).has_type_arguments = true;
            for (auto& p : *params) x3::_val(ctx).type_arguments.push_back(p);
        }
        auto& ret = fusion::at_c<1>(attr);
        if (ret) x3::_val(ctx).type_arguments.push_back(*ret);
    }
};

auto const atomic_type_ann = x3::rule<class ata_tag, type_annotation>{} =
    fnc_type_params[set_fnc_type_ann{}] | unit_type_ann |
    (ast_ident >> x3::matches['?'] >> -('[' >> (qualified_type_arg % ',') >> ']')
    )[set_atomic_type{}];

struct set_union_type {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        auto& first = fusion::at_c<0>(attr);
        auto& rest = fusion::at_c<1>(attr);
        if (rest.empty()) {
            // Not a union — just pass through the atomic type
            x3::_val(ctx) = first;
        } else {
            x3::_val(ctx).is_union = true;
            x3::_val(ctx).union_types.push_back(first);
            for (auto& t : rest) {
                x3::_val(ctx).union_types.push_back(t);
            }
        }
    }
};

struct set_intersection_or_union {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        auto& base = fusion::at_c<0>(attr);
        auto& maybe_union = fusion::at_c<1>(attr);
        auto& maybe_intersection = fusion::at_c<2>(attr);

        if (!maybe_union.empty()) {
            x3::_val(ctx).is_union = true;
            x3::_val(ctx).union_types.push_back(base);
            for (auto& t : maybe_union) {
                x3::_val(ctx).union_types.push_back(t);
            }
        } else if (!maybe_intersection.empty()) {
            x3::_val(ctx).is_intersection = true;
            x3::_val(ctx).intersection_types.push_back(base);
            for (auto& t : maybe_intersection) {
                x3::_val(ctx).intersection_types.push_back(t);
            }
        } else {
            x3::_val(ctx) = base;
        }
    }
};

// Full type annotation: atomic_type (| atomic_type)* or atomic_type (& atomic_type)*
auto const type_ann_def =
    (atomic_type_ann >> *('|' >> atomic_type_ann) >> *('&' >> atomic_type_ann)
    )[set_intersection_or_union{}];
BOOST_SPIRIT_DEFINE(type_ann)

// ============================================================================
// Lambda expressions
// ============================================================================

// Block lambda: { params -> body } or { body }
auto const lambda_param = x3::rule<class lp_tag, lambda_parameter>{} =
    ast_ident >> x3::matches[':' >> type_ann];

// We need a custom action because lambda_parameter has (name, has_type, type)
// but x3::matches produces bool, and the type is optional
struct set_lambda_param {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).name = fusion::at_c<0>(attr);
        auto& maybe_type = fusion::at_c<1>(attr);
        if (maybe_type) {
            x3::_val(ctx).has_type = true;
            x3::_val(ctx).type = *maybe_type;
        }
    }
};

auto const lambda_param_typed = x3::rule<class lpt_tag, lambda_parameter>{} =
    (ast_ident >> -(':' >> type_ann))[set_lambda_param{}];

struct set_block_lambda {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        auto& maybe_params = fusion::at_c<0>(attr);
        if (maybe_params) x3::_val(ctx).parameters = *maybe_params;
        auto& stmts = fusion::at_c<1>(attr);
        if (stmts.size() == 1) {
            x3::_val(ctx).body = stmts[0];
        } else {
            list_expression le;
            for (auto& s : stmts) le.elements.push_back(s);
            x3::_val(ctx).body = expression(le);
        }
        x3::_val(ctx).is_block = true;
    }
};

// Block lambda: { params -> body } — params are optional, body is 1+ statements
auto const lambda_rule_def =
    ('{' >> -((lambda_param_typed % ',') >> "->") >> +statement_rule >> '}')[set_block_lambda{}];
BOOST_SPIRIT_DEFINE(lambda_rule)

// ============================================================================
// Anonymous object literal: { key: value, key2: value2 }
// ============================================================================

auto const obj_field = x3::rule<class of_tag, anonymous_object_field>{} =
    ast_ident >> ':' >> expression_rule;

struct set_obj_lit {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        x3::_val(ctx).fields = x3::_attr(ctx);
        x3::_val(ctx).is_map = false;
    }
};

// Lookahead: object literal starts with { ident : (not { ident ... })
// Use raw character matching to avoid attribute synthesis issues
auto const obj_lit_lookahead = &(x3::lit('{') >> (x3::alpha | x3::char_('_')) >> *(x3::alnum | x3::char_('-') | x3::char_('_')) >> ':');

auto const obj_lit = x3::rule<class ol_tag, anonymous_object_literal>{} =
    (obj_lit_lookahead >> '{' >> (obj_field % ',') >> -x3::lit(',') >> '}')[set_obj_lit{}];

// ============================================================================
// Expressions (precedence climbing)
// ============================================================================

auto const paren_expr = x3::rule<class paren_tag, expression>{} =
    '(' >> expression_rule >> ')';

// List/array literal: [a, b, c]
auto const array_lit = x3::rule<class arr_tag, anonymous_array_literal>{} =
    '[' >> -(expression_rule % ',') >> -x3::lit(',') >> ']';

// Tuple literal: (a, b) or (x = 10, y = 20)
// Single element without comma = grouped expression (paren_expr handles that)
struct set_tuple_elem {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        auto& maybe_name = fusion::at_c<0>(attr);
        if (maybe_name) {
            x3::_val(ctx).name = maybe_name->name;
            x3::_val(ctx).is_named = true;
        }
        x3::_val(ctx).value = fusion::at_c<1>(attr);
    }
};

auto const tuple_elem = x3::rule<class te_tag, tuple_element>{} =
    (-(ast_ident >> '=') >> expression_rule)[set_tuple_elem{}];

struct set_tuple_lit {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& elems = x3::_attr(ctx);
        // 1 unnamed element = grouped expression, not a tuple — reject here, paren_expr handles it
        if (elems.size() == 1 && !elems[0].is_named) {
            x3::_pass(ctx) = false;
            return;
        }
        x3::_val(ctx).elements = elems;
    }
};

auto const tuple_lit = x3::rule<class tl_tag, tuple_literal>{} =
    ('(' >> (tuple_elem % ',') >> -x3::lit(',') >> ')')[set_tuple_lit{}];

// Initialization block: TypeName { field = value, ... }
auto const init_param = x3::rule<class ip_tag, named_parameter>{} =
    ast_ident >> '=' >> expression_rule;

struct set_init_block {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).type_name = fusion::at_c<0>(attr);
        x3::_val(ctx).parameters = fusion::at_c<1>(attr);
    }
};

auto const init_block = x3::rule<class ib_tag, initialization_block>{} =
    (ast_ident >> '{' >> (init_param % ',') >> -x3::lit(',') >> '}')[set_init_block{}];

// Primary: try multiline/template strings before regular, float before integer
// init_block before plain identifier (both start with identifier)
// Anonymous function: fnc(params) -> type { body }
struct set_anon_func {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        lambda_expression le;
        auto& params = fusion::at_c<0>(attr);
        if (params) le.parameters = *params;
        auto& body_stmts = fusion::at_c<1>(attr);
        if (body_stmts.size() == 1) {
            le.body = body_stmts[0];
        } else {
            list_expression list;
            for (auto& s : body_stmts) list.elements.push_back(s);
            le.body = expression(list);
        }
        le.is_block = true;
        x3::_val(ctx) = expression(le);
    }
};
auto const anon_fnc = x3::rule<class afnc_tag, expression>{} =
    (kw_fnc >> '(' >> -(lambda_param_typed % ',') >> ')' >> -("->" >> x3::omit[type_ann]) >> '{' >> +statement_rule >> '}')[set_anon_func{}];

auto const primary_expr_def =
    bool_lit | multiline_string_lit | template_multiline_lit | string_lit | template_string_lit |
    float_lit | integer_lit |
    array_lit | tuple_lit | paren_expr | old_rule |
    handle_rule | obj_lit | lambda_rule | tuple_destr_rule |
    anon_fnc | intersect_impl | anon_impl_rule | ast_ident;

// ============================================================================
// Postfix operators: .field, [index], ?.field, (args)
// ============================================================================

// Postfix operation variants
struct postfix_dot {
    std::string field;
    bool is_numeric = false;
};

struct postfix_index {
    expression idx;
};

struct postfix_safe_nav {
    std::string field;
};

struct postfix_safe_index {
    expression idx;
};

struct postfix_safe_call {
    std::vector<expression> args;
};

struct postfix_call_args {
    std::vector<expression> args;
};

// Postfix unary operators: !!, ?!, ?
struct postfix_unary_op {
    std::string op;
};

// Brace block after identifier: Name { field = value } or Name { fnc ... }
struct postfix_brace {
    std::vector<named_parameter> init_params;
    std::vector<handler_function> methods;
    bool has_methods = false;
};

using postfix_op = boost::variant<postfix_dot, postfix_index, postfix_safe_nav, postfix_safe_index, postfix_safe_call, postfix_call_args, postfix_unary_op, postfix_brace>;

struct make_postfix_dot {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& raw = x3::_attr(ctx);
        std::string s(raw.begin(), raw.end());
        postfix_dot d;
        d.field = s;
        d.is_numeric = !s.empty() && std::isdigit(static_cast<unsigned char>(s[0]));
        x3::_val(ctx) = postfix_op(d);
    }
};

struct make_postfix_index {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        postfix_index pi;
        pi.idx = x3::_attr(ctx);
        x3::_val(ctx) = postfix_op(pi);
    }
};

struct make_postfix_safe_nav {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& raw = x3::_attr(ctx);
        postfix_safe_nav sn;
        sn.field = std::string(raw.begin(), raw.end());
        x3::_val(ctx) = postfix_op(sn);
    }
};

struct make_postfix_safe_index {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        postfix_safe_index si;
        si.idx = x3::_attr(ctx);
        x3::_val(ctx) = postfix_op(si);
    }
};

struct make_postfix_safe_call {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        postfix_safe_call sc;
        auto& maybe_args = x3::_attr(ctx);
        if (maybe_args) sc.args = *maybe_args;
        x3::_val(ctx) = postfix_op(sc);
    }
};

struct make_postfix_call {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        postfix_call_args pc;
        auto& maybe_args = x3::_attr(ctx);
        if (maybe_args) pc.args = *maybe_args;
        x3::_val(ctx) = postfix_op(pc);
    }
};

struct make_postfix_unary {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& raw = x3::_attr(ctx);
        postfix_unary_op pu;
        pu.op = std::string(raw.begin(), raw.end());
        x3::_val(ctx) = postfix_op(pu);
    }
};

struct make_postfix_brace_init {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        postfix_brace pb;
        pb.init_params = x3::_attr(ctx);
        pb.has_methods = false;
        x3::_val(ctx) = postfix_op(pb);
    }
};

auto const postfix_op_rule = x3::rule<class po_tag, postfix_op>{} =
    (x3::lexeme[x3::lit("?.") >> x3::raw[ ident_start >> *ident_body ]])[make_postfix_safe_nav{}] |
    ("?[" >> expression_rule >> ']')[make_postfix_safe_index{}] |
    ("?(" >> -(expression_rule % ',') >> ')')[make_postfix_safe_call{}] |
    ('.' >> x3::lexeme[ x3::raw[ x3::digit >> *x3::digit | ident_start >> *ident_body ] ])[make_postfix_dot{}] |
    ('[' >> expression_rule >> ']')[make_postfix_index{}] |
    ('(' >> -(expression_rule % ',') >> ')')[make_postfix_call{}] |
    ('{' >> (init_param % ',') >> -x3::lit(',') >> '}')[make_postfix_brace_init{}] |
    (&(x3::lit('{')) >> lambda_rule)[([](auto& ctx) {
        postfix_call_args pc;
        pc.args.push_back(expression(x3::_attr(ctx)));
        x3::_val(ctx) = pc;
    })] |
    (x3::lexeme[ x3::raw[ x3::lit("!!") | x3::lit("?!") | (x3::lit("?") >> !x3::char_(":.[(")) ] ])[make_postfix_unary{}];

struct fold_postfix {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        expression result = fusion::at_c<0>(attr);
        auto& ops = fusion::at_c<1>(attr);
        for (auto& op : ops) {
            if (auto* d = boost::get<postfix_dot>(&op)) {
                tuple_indexing ti;
                ti.tuple = result;
                ti.index = d->field;
                ti.is_numeric = d->is_numeric;
                result = expression(ti);
            } else if (auto* pi = boost::get<postfix_index>(&op)) {
                array_indexing ai;
                ai.array = result;
                ai.index = pi->idx;
                result = expression(ai);
            } else if (auto* sn = boost::get<postfix_safe_nav>(&op)) {
                safe_navigation_expression sne;
                sne.nullable_expr = result;
                sne.field_name = sn->field;
                result = expression(sne);
            } else if (auto* si = boost::get<postfix_safe_index>(&op)) {
                // ?[] safe indexing — parsed as array_indexing on the nullable expr
                // The safe_chaining_pass detects nullable receiver and handles short-circuit
                array_indexing ai;
                ai.array = result;
                ai.index = si->idx;
                result = expression(ai);
            } else if (auto* sc = boost::get<postfix_safe_call>(&op)) {
                // ?() safe invocation — parsed as function_call
                function_call call;
                auto* id_ptr = boost::get<identifier>(&result);
                if (id_ptr) call.function_name = *id_ptr;
                for (auto& a : sc->args) call.arguments.push_back(a);
                result = expression(call);
            } else if (auto* pc = boost::get<postfix_call_args>(&op)) {
                auto* id_ptr = boost::get<identifier>(&result);
                auto* ti_fwd = boost::get<x3::forward_ast<tuple_indexing>>(&result);
                if (id_ptr) {
                    function_call call;
                    call.function_name = *id_ptr;
                    for (auto& a : pc->args) call.arguments.push_back(a);
                    result = expression(call);
                } else if (ti_fwd && !ti_fwd->get().is_numeric) {
                    auto& ti = ti_fwd->get();
                    auto* base_id = boost::get<identifier>(&ti.tuple.get());
                    if (base_id) {
                        implicit_effect_call call;
                        call.effect_name = *base_id;
                        call.operation_name.name = ti.index;
                        for (auto& a : pc->args) call.arguments.push_back(a);
                        result = expression(call);
                    } else {
                        function_call call;
                        call.function_name.name = ti.index;
                        call.arguments.push_back(ti.tuple.get());
                        for (auto& a : pc->args) call.arguments.push_back(a);
                        result = expression(call);
                    }
                }
            } else if (auto* pu = boost::get<postfix_unary_op>(&op)) {
                unary_operation uo;
                uo.op = pu->op;
                uo.operand = result;
                result = expression(uo);
            } else if (auto* pb = boost::get<postfix_brace>(&op)) {
                // Name { field = value } → initialization_block
                auto* id_ptr = boost::get<identifier>(&result);
                if (id_ptr) {
                    initialization_block ib;
                    ib.type_name = *id_ptr;
                    ib.parameters = pb->init_params;
                    result = expression(ib);
                }
            }
        }
        x3::_val(ctx) = result;
    }
};

auto const postfix_expr =
    (primary_expr >> *postfix_op_rule)[fold_postfix{}];

auto const unary_expr_def =
    ('-' >> unary_expr)[make_unary_neg{}] |
    ('!' >> unary_expr)[make_unary_not{}] |
    (primary_expr >> *postfix_op_rule)[fold_postfix{}];

auto const mult_expr_def =
    (unary_expr >> *(x3::char_("*/%") >> unary_expr))[fold_char_binop{}];

auto const additive_expr_def =
    (mult_expr >> *(x3::char_("+-") >> mult_expr))[fold_char_binop{}];

auto const cmp_op = x3::rule<class cop_tag, std::string>{} =
    x3::raw[ x3::string("==") | x3::string("!=") |
             x3::string("<=") | x3::string(">=") |
             x3::string("<")  | x3::string(">") ];
auto const comparison_expr_def =
    (additive_expr >> *(cmp_op >> additive_expr))[fold_string_binop{}];

auto const logical_and_op = x3::string("&&");
auto const logical_and_expr_def =
    (comparison_expr >> *(logical_and_op >> comparison_expr))[fold_string_binop{}];

auto const logical_or_op = x3::string("||");
auto const logical_or_expr_def =
    (logical_and_expr >> *(logical_or_op >> logical_and_expr))[fold_string_binop{}];

// Elvis operator: x ?? default (null-coalescing)
struct fold_elvis {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        expression result = fusion::at_c<0>(attr);
        auto& rest = fusion::at_c<1>(attr);
        for (auto& rhs : rest) {
            elvis_expression ev;
            ev.nullable_expr = result;
            ev.default_value = rhs;
            result = expression(ev);
        }
        x3::_val(ctx) = result;
    }
};

x3::rule<class elvis_tag, expression> const elvis_expr = "elvis";
auto const elvis_expr_def =
    (logical_or_expr >> *("?:" >> logical_or_expr))[fold_elvis{}];

// Pipeline operator: x |> f (lowest precedence)
struct fold_pipeline {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        expression result = fusion::at_c<0>(attr);
        auto& rest = fusion::at_c<1>(attr);
        for (auto& rhs : rest) {
            pipeline_expression pe;
            pe.value = result;
            pe.function = rhs;
            result = expression(pe);
        }
        x3::_val(ctx) = result;
    }
};

x3::rule<class pipe_tag, expression> const pipeline_expr = "pipeline";
auto const pipeline_expr_def =
    (elvis_expr >> *("|>" >> elvis_expr))[fold_pipeline{}];

auto const expression_rule_def = pipeline_expr;

BOOST_SPIRIT_DEFINE(expression_rule, pipeline_expr, elvis_expr, logical_or_expr, logical_and_expr,
                    comparison_expr, additive_expr, mult_expr,
                    unary_expr, primary_expr)

// ============================================================================
// Statements
// ============================================================================

auto const return_stmt = x3::rule<class ret_tag, return_statement>{} =
    (kw_rtn >> -expression_rule)[set_return{}];

auto const val_decl = x3::rule<class val_tag, val_declaration>{} =
    (kw_val >> ast_ident >> -(':' >> type_ann) >> '=' >> expression_rule)[set_val_decl{}];

auto const var_decl = x3::rule<class var_tag, var_declaration>{} =
    (kw_var >> ast_ident >> -(':' >> type_ann) >> '=' >> expression_rule)[set_var_decl{}];

// Assignment: ident = expr (but not == which is comparison)
struct set_assignment {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        named_return_assignment a;
        a.return_name = fusion::at_c<0>(attr);
        a.value = fusion::at_c<1>(attr);
        x3::_val(ctx) = expression(a);
    }
};
auto const assign_stmt = x3::rule<class asgn_tag, expression>{} =
    (ast_ident >> '=' >> !x3::lit('=') >> expression_rule)[set_assignment{}];

// Field assignment: obj.field = expr
struct set_field_assignment {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        binary_operation bin;
        bin.op = "=";
        tuple_indexing ti;
        ti.tuple = expression(fusion::at_c<0>(attr));
        ti.index = fusion::at_c<1>(attr).name;
        bin.left = expression(ti);
        bin.right = fusion::at_c<2>(attr);
        x3::_val(ctx) = expression(bin);
    }
};
auto const field_assign_stmt = x3::rule<class fasgn_tag, expression>{} =
    (ast_ident >> '.' >> ast_ident >> '=' >> !x3::lit('=') >> expression_rule)[set_field_assignment{}];

auto const statement_rule_def = return_stmt | val_decl | var_decl | field_assign_stmt | assign_stmt | func_def | expression_rule;
BOOST_SPIRIT_DEFINE(statement_rule)

auto const block_rule_def =
    ('{' >> *statement_rule >> '}')[set_block{}];
BOOST_SPIRIT_DEFINE(block_rule)

// ============================================================================
// Annotations: @name or @name(key: value, ...)
// ============================================================================

struct set_annotation {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& raw = x3::_attr(ctx);
        x3::_val(ctx).name = std::string(raw.begin(), raw.end());
    }
};

// Simple annotation: @name or @name(...)
// The name is captured; args are stored if present.
auto const annotation_rule = x3::rule<class ann_tag, annotation>{} =
    ('@' >> x3::lexeme[ x3::raw[ ident_start >> *ident_body ] ]
    )[set_annotation{}] >> -(x3::omit['(' >> *(x3::char_ - ')') >> ')']);

// ============================================================================
// Function definition
// ============================================================================

auto const func_param_mut = x3::rule<class fpm_tag, function_parameter>{} =
    (kw_var >> ast_ident >> ':' >> type_ann)[set_func_param_mut{}];

auto const func_param_imm = x3::rule<class fpi_tag, function_parameter>{} =
    (ast_ident >> ':' >> type_ann)[set_func_param{}];

auto const func_param = x3::rule<class fp_tag, function_parameter>{} =
    func_param_mut | func_param_imm;

// Optional generic type parameters on functions: fnc name[T, U](...)
auto const func_generic_params = '[' >> x3::omit[x3::lexeme[ident_start >> *ident_body] % ','] >> ']';

// Optional annotation(s) before function definition
// @uses(Effect.op, Effect.op) is captured into a thread-local for set_func_def to consume.
// Other annotations are discarded.
static thread_local std::vector<std::string> parsed_uses_effects_;

struct capture_uses_annotation {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        parsed_uses_effects_.clear();
        auto& raw = x3::_attr(ctx);
        // raw is the string inside parens: "Console.println, FileSystem.read"
        std::string current;
        for (char c : raw) {
            if (c == ',') {
                auto s = current.find_first_not_of(" \t");
                auto e = current.find_last_not_of(" \t");
                if (s != std::string::npos)
                    parsed_uses_effects_.push_back(current.substr(s, e - s + 1));
                current.clear();
            } else {
                current += c;
            }
        }
        auto s = current.find_first_not_of(" \t");
        auto e = current.find_last_not_of(" \t");
        if (s != std::string::npos)
            parsed_uses_effects_.push_back(current.substr(s, e - s + 1));
    }
};

auto const uses_annotation =
    x3::lit('@') >> x3::lit("uses") >> '(' >> x3::lexeme[*(x3::char_ - ')')][capture_uses_annotation{}] >> ')';

auto const other_annotation =
    '@' >> x3::lexeme[ ident_start >> *ident_body ] >> -('(' >> *(x3::char_ - ')') >> ')');

auto const opt_annotations = *(uses_annotation | x3::omit[other_annotation]);

struct set_func_def_with_effects {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).name = fusion::at_c<0>(attr);
        auto& params = fusion::at_c<1>(attr);
        if (params) x3::_val(ctx).parameters = *params;
        auto& ret = fusion::at_c<2>(attr);
        if (ret) { x3::_val(ctx).has_return_type = true; x3::_val(ctx).return_type = *ret; }
        x3::_val(ctx).body = fusion::at_c<3>(attr);
        // Consume parsed @uses effects
        if (!parsed_uses_effects_.empty()) {
            x3::_val(ctx).has_effects = true;
            for (auto& e : parsed_uses_effects_)
                x3::_val(ctx).effects_clause.push_back(identifier{e});
            parsed_uses_effects_.clear();
        }
    }
};

struct set_func_def_mut_with_effects {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).is_mutating = true;
        x3::_val(ctx).name = fusion::at_c<0>(attr);
        auto& params = fusion::at_c<1>(attr);
        if (params) x3::_val(ctx).parameters = *params;
        auto& ret = fusion::at_c<2>(attr);
        if (ret) { x3::_val(ctx).has_return_type = true; x3::_val(ctx).return_type = *ret; }
        x3::_val(ctx).body = fusion::at_c<3>(attr);
        if (!parsed_uses_effects_.empty()) {
            x3::_val(ctx).has_effects = true;
            for (auto& e : parsed_uses_effects_)
                x3::_val(ctx).effects_clause.push_back(identifier{e});
            parsed_uses_effects_.clear();
        }
    }
};

auto const func_def_var = x3::rule<class fdv_tag, function_definition>{} =
    (x3::omit[opt_annotations] >> kw_var >> kw_fnc >> ast_ident >> -func_generic_params >> '(' >> -(func_param % ',') >> ')' >>
     -("->" >> type_ann) >> block_rule)[set_func_def_mut_with_effects{}];

auto const func_def_imm = x3::rule<class fdi_tag, function_definition>{} =
    (x3::omit[opt_annotations] >> kw_fnc >> ast_ident >> -func_generic_params >> '(' >> -(func_param % ',') >> ')' >>
     -("->" >> type_ann) >> block_rule)[set_func_def_with_effects{}];

auto const func_def_def = func_def_var | func_def_imm;
BOOST_SPIRIT_DEFINE(func_def)

// ============================================================================
// Operator declaration: opr +(a: int, b: int) -> int { body }
// ============================================================================

auto const kw_opr = x3::lexeme[ x3::lit("opr") >> !ident_body ];

// Operator symbol: one or more operator characters
auto const op_symbol = x3::rule<class ops_tag, std::string>{} =
    x3::lexeme[ x3::raw[ +x3::char_("+*/%<>=!&|^~?:@#-") ] ];

struct set_opr_def {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).symbol = fusion::at_c<0>(attr);
        auto& params = fusion::at_c<1>(attr);
        if (params) x3::_val(ctx).parameters = *params;
        auto& ret = fusion::at_c<2>(attr);
        if (ret) { x3::_val(ctx).has_return_type = true; x3::_val(ctx).return_type = *ret; }
        x3::_val(ctx).body = fusion::at_c<3>(attr);
    }
};

auto const opr_def = x3::rule<class opr_tag, operator_function>{} =
    (kw_opr >> op_symbol >> '(' >> -(func_param % ',') >> ')' >>
     -("->" >> type_ann) >> block_rule)[set_opr_def{}];

// ============================================================================
// Generic type parameters: [T, out U, V: Bound]
// ============================================================================

struct set_generic_param {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        auto& maybe_variance = fusion::at_c<0>(attr);
        x3::_val(ctx).name = fusion::at_c<1>(attr);
        if (maybe_variance) x3::_val(ctx).variance = *maybe_variance;
        auto& maybe_bound = fusion::at_c<2>(attr);
        if (maybe_bound) {
            x3::_val(ctx).has_bound = true;
            x3::_val(ctx).bound = *maybe_bound;
        }
    }
};

auto const variance_kw = x3::rule<class vk_tag, std::string>{} =
    x3::lexeme[ x3::raw[ (x3::lit("in") | x3::lit("out")) >> !ident_body ] ];

auto const generic_param = x3::rule<class gp_tag, generic_type_parameter>{} =
    (-variance_kw >> ast_ident >> -(':' >> type_ann))[set_generic_param{}];

auto const generic_params = '[' >> (generic_param % ',') >> ']';

// ============================================================================
// Field declaration: val/var name: Type
// ============================================================================

struct set_field_decl {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        // attr is: (bool_from_val_or_var, identifier, type_annotation)
        // The bool comes from kw_val->false or kw_var->true
        auto& is_var_variant = fusion::at_c<0>(attr);
        bool is_var = boost::get<bool>(&is_var_variant) ? boost::get<bool>(is_var_variant) : false;
        x3::_val(ctx).is_mutable = is_var;
        x3::_val(ctx).has_explicit_val = !is_var;
        x3::_val(ctx).name = fusion::at_c<1>(attr);
        x3::_val(ctx).type = fusion::at_c<2>(attr);
    }
};

auto const field_mutability = (kw_val >> x3::attr(false)) | (kw_var >> x3::attr(true));

auto const field_decl = x3::rule<class fd_tag, field_declaration>{} =
    (field_mutability >> ast_ident >> ':' >> type_ann)[set_field_decl{}];

// ============================================================================
// Unified type definition: <kind> <name> [generics] { members }
// Examples: class Person { }, struct Point { }, enum Color { }, type data { }
// ============================================================================

struct set_enum_variant {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).name = fusion::at_c<0>(attr);
        auto& maybe_fields = fusion::at_c<1>(attr);
        if (maybe_fields) {
            x3::_val(ctx).has_associated_data = true;
            x3::_val(ctx).associated_fields = *maybe_fields;
        }
    }
};

auto const enum_variant_rule = x3::rule<class ev_tag, enum_variant>{} =
    (ast_ident >> -('(' >> (field_decl % ',') >> ')'))[set_enum_variant{}];

// A member inside { } is a field, variant, or method definition
// Fields: val/var name: Type
// Variants: Name or Name(fields)  
// Methods: fnc name() -> Type { body } or var fnc name() { body }
auto const type_member = field_decl | enum_variant_rule;

// Function signature (no body) — for trait abstract methods
struct set_func_sig {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).name = fusion::at_c<0>(attr);
        auto& params = fusion::at_c<1>(attr);
        if (params) x3::_val(ctx).parameters = *params;
        auto& ret = fusion::at_c<2>(attr);
        if (ret) { x3::_val(ctx).has_return_type = true; x3::_val(ctx).return_type = *ret; }
    }
};
auto const func_sig = x3::rule<class fsig_tag, function_definition>{} =
    (kw_fnc >> ast_ident >> '(' >> -(func_param % ',') >> ')' >>
     -("->" >> type_ann))[set_func_sig{}];

// Method inside a type body — reuse func_def (supports var fnc too)
// func_def has body, func_sig doesn't — try func_def first
auto const type_body_item = annotation_rule | func_def | func_sig | type_member;

// ── Pattern 1: Meta-definition ── type <name> { body }
// Defines a new type kind. Body contains arbitrary expressions (meta_set calls).
struct set_meta_type_def {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).kind.name = "type";
        x3::_val(ctx).name = fusion::at_c<0>(attr);
        auto& stmts = fusion::at_c<1>(attr);
        for (auto& s : stmts) {
            x3::_val(ctx).body.push_back(s);
        }
    }
};

x3::rule<class metatd_tag, type_definition> const meta_type_def = "meta_type_definition";
auto const meta_type_def_def =
    (kw_type >> ast_ident >> '{' >> *statement_rule >> '}')[set_meta_type_def{}];
BOOST_SPIRIT_DEFINE(meta_type_def)

// ── Pattern 2: Instance declaration ── <kind> <name> [generics] { members }
// Uses a previously-defined type kind. Parser is kind-agnostic.
// Members can be fields, variants, or method definitions.
// Empty body {} is a forward declaration.
struct set_instance_type_def {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).kind = fusion::at_c<0>(attr);
        x3::_val(ctx).name = fusion::at_c<1>(attr);
        auto& gp = fusion::at_c<2>(attr);
        if (gp) x3::_val(ctx).type_parameters = *gp;
        auto& members = fusion::at_c<3>(attr);
        for (auto& m : members) {
            if (auto* ev = boost::get<enum_variant>(&m)) {
                x3::_val(ctx).variants.push_back(*ev);
            } else if (auto* fd = boost::get<field_declaration>(&m)) {
                x3::_val(ctx).fields.push_back(*fd);
            } else if (auto* fn = boost::get<function_definition>(&m)) {
                x3::_val(ctx).body.push_back(expression(*fn));
            } else if (auto* ann = boost::get<annotation>(&m)) {
                // Store annotation as expression for MMS processing
                // MMS will attach it to the next declaration
                (void)ann;  // Annotations processed by MMS, not parser
            }
        }
    }
};

x3::rule<class insttd_tag, type_definition> const instance_type_def = "instance_type_definition";
auto const instance_type_def_def =
    (ast_ident >> ast_ident >> -generic_params >>
     '{' >> *(type_body_item >> -x3::lit(',')) >> '}'
    )[set_instance_type_def{}];
BOOST_SPIRIT_DEFINE(instance_type_def)

// ============================================================================
// Typealias: typealias Name = Type
// ============================================================================

auto const typealias_decl = x3::rule<class ta_decl_tag, typealias_declaration>{} =
    kw_typealias >> ast_ident >> '=' >> type_ann;

// ============================================================================
// Newtype: newtype Name = Type
// ============================================================================

struct set_newtype {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).wrapper_name = fusion::at_c<0>(attr);
        x3::_val(ctx).wrapped_type = fusion::at_c<1>(attr);
        x3::_val(ctx).is_transparent = true;
    }
};

auto const newtype_decl = x3::rule<class nt_tag, newtype_declaration>{} =
    (kw_newtype >> ast_ident >> '=' >> type_ann)[set_newtype{}];

// ============================================================================
// Type alias arrow syntax: type Name -> Type [where { predicate }]
// Produces typealias_declaration or refinement_type_definition
// ============================================================================

struct set_type_arrow {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        auto& name = fusion::at_c<0>(attr);
        auto& base = fusion::at_c<1>(attr);
        auto& maybe_pred = fusion::at_c<2>(attr);
        if (maybe_pred) {
            refinement_type_definition ref;
            ref.name = name;
            ref.base_type = base;
            ref.predicate = *maybe_pred;
            x3::_val(ctx) = expression(ref);
        } else {
            typealias_declaration ta;
            ta.alias_name = name;
            ta.target_type = base;
            x3::_val(ctx) = expression(ta);
        }
    }
};

auto const type_arrow_decl = x3::rule<class tarr_tag, expression>{} =
    (kw_type >> ast_ident >> "->" >> type_ann >>
     -(kw_where >> '{' >> expression_rule >> '}')
    )[set_type_arrow{}];

// ============================================================================
// Imp declaration: imp std.math | imp m = std.math | imp { sin, cos } = std.math
// ============================================================================

// Module path: ident.ident.ident
auto const module_path = x3::rule<class mp_tag, std::vector<std::string>>{} =
    x3::lexeme[ x3::raw[ ident_start >> *ident_body ] ] % '.';

struct set_imp_basic {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        x3::_val(ctx).import_type = ImportType::IMP_BASIC;
        x3::_val(ctx).namespace_path = x3::_attr(ctx);
        x3::_val(ctx).is_imp = true;
    }
};

struct set_imp_aliased {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).import_type = ImportType::IMP_ALIASED;
        auto& alias_id = fusion::at_c<0>(attr);
        x3::_val(ctx).alias = alias_id.name;
        x3::_val(ctx).has_alias = true;
        x3::_val(ctx).namespace_path = fusion::at_c<1>(attr);
        x3::_val(ctx).is_imp = true;
    }
};

struct set_imp_symbol {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        auto& orig = fusion::at_c<0>(attr);
        x3::_val(ctx).original_name = orig.name;
        auto& maybe_alias = fusion::at_c<1>(attr);
        if (maybe_alias) {
            x3::_val(ctx).local_name = maybe_alias->name;
            x3::_val(ctx).has_alias = true;
        } else {
            x3::_val(ctx).local_name = orig.name;
        }
    }
};

struct set_imp_destructured {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).import_type = ImportType::IMP_DESTRUCTURED;
        x3::_val(ctx).symbols = fusion::at_c<0>(attr);
        x3::_val(ctx).namespace_path = fusion::at_c<1>(attr);
        x3::_val(ctx).is_imp = true;
    }
};

auto const imp_symbol = x3::rule<class is_tag, imp_symbol_mapping>{} =
    (ast_ident >> -("->" >> ast_ident))[set_imp_symbol{}];

auto const imp_destructured = x3::rule<class id_tag, import_declaration>{} =
    (kw_imp >> '{' >> (imp_symbol % ',') >> '}' >> '=' >> module_path)[set_imp_destructured{}];

auto const imp_aliased = x3::rule<class ia_tag, import_declaration>{} =
    (kw_imp >> ast_ident >> '=' >> module_path)[set_imp_aliased{}];

auto const imp_basic = x3::rule<class ib_tag, import_declaration>{} =
    (kw_imp >> module_path)[set_imp_basic{}];

// Order matters: try destructured first, then aliased, then basic
auto const imp_decl = imp_destructured | imp_aliased | imp_basic;

// ============================================================================
// Namespace: namespace com.example { ... }
// ============================================================================

struct set_namespace {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).name_parts = fusion::at_c<0>(attr);
        auto& body = fusion::at_c<1>(attr);
        for (auto& e : body) {
            x3::_val(ctx).body.push_back(e);
        }
    }
};

auto const namespace_def_def =
    (kw_namespace >> module_path >> '{' >> *toplevel_item >> '}')[set_namespace{}];
BOOST_SPIRIT_DEFINE(namespace_def)

// ============================================================================
// Extension block: extend TypeName { fnc method() -> RetType { body } }
// ============================================================================

auto const kw_extend = x3::lexeme[ x3::lit("extend") >> !ident_body ];

struct set_ext_method {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).name = fusion::at_c<0>(attr);
        auto& params = fusion::at_c<1>(attr);
        if (params) x3::_val(ctx).parameters = *params;
        auto& ret = fusion::at_c<2>(attr);
        if (ret) { x3::_val(ctx).has_return_type = true; x3::_val(ctx).return_type = *ret; }
        x3::_val(ctx).body = fusion::at_c<3>(attr);
    }
};

auto const ext_method = x3::rule<class em_tag, extension_method>{} =
    (kw_fnc >> ast_ident >> '(' >> -(func_param % ',') >> ')' >>
     -("->" >> type_ann) >> block_rule)[set_ext_method{}];

struct set_ext_block {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).target_type = fusion::at_c<0>(attr);
        x3::_val(ctx).methods = fusion::at_c<1>(attr);
    }
};

auto const extension_def_def =
    (kw_extend >> type_ann >> '{' >> *ext_method >> '}')[set_ext_block{}];
BOOST_SPIRIT_DEFINE(extension_def)

// ============================================================================
// Effects system
// ============================================================================

auto const kw_effect = x3::lexeme[ x3::lit("effect") >> !ident_body ];
auto const kw_handle = x3::lexeme[ x3::lit("handle") >> !ident_body ];
auto const kw_resume = x3::lexeme[ x3::lit("resume") >> !ident_body ];

// Effect operation: fnc op(params) -> RetType
struct set_effect_op {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).name = fusion::at_c<0>(attr);
        auto& params = fusion::at_c<1>(attr);
        if (params) x3::_val(ctx).parameters = *params;
        auto& ret = fusion::at_c<2>(attr);
        if (ret) { x3::_val(ctx).has_return_type = true; x3::_val(ctx).return_type = *ret; }
    }
};

auto const effect_op = x3::rule<class eop_tag, effect_operation>{} =
    (kw_fnc >> ast_ident >> '(' >> -(func_param % ',') >> ')' >>
     -("->" >> type_ann))[set_effect_op{}];

// Effect definition: effect Name { fnc op(params) -> RetType }
auto const effect_def = x3::rule<class edef_tag, effect_definition>{} =
    kw_effect >> ast_ident >> '{' >> *effect_op >> '}';

// Handler function inside inline trait impl
struct set_handler_func {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).name = fusion::at_c<0>(attr);
        auto& params = fusion::at_c<1>(attr);
        if (params) x3::_val(ctx).parameters = *params;
        // Skip optional return type (at_c<2>) — handler_function has no return_type field
        x3::_val(ctx).body = fusion::at_c<3>(attr);
    }
};

auto const handler_func = x3::rule<class hf_tag, handler_function>{} =
    (kw_fnc >> ast_ident >> '(' >> -(func_param % ',') >> ')' >>
     -("->" >> type_ann) >> block_rule)[set_handler_func{}];

// Inline trait impl: TraitName { fnc method() { body } }
auto const inline_impl = x3::rule<class ii_tag, inline_trait_impl>{} =
    ast_ident >> '{' >> +handler_func >> '}';

// Handle expression: handle(computation: { body }) { Effect { fnc op() { ... } } }
// Simplified: handle({ body }) { handlers... }
struct set_handle_expr {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).body = fusion::at_c<0>(attr);
        x3::_val(ctx).handlers = fusion::at_c<1>(attr);
    }
};

// handle Name { fnc ... } — top-level handler registration (empty body)
struct set_handle_decl {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        inline_trait_impl impl;
        impl.trait_name = fusion::at_c<0>(attr);
        impl.methods = fusion::at_c<1>(attr);
        x3::_val(ctx).body = block_expression{};
        x3::_val(ctx).handlers.push_back(impl);
    }
};

auto const handle_rule_def =
    (kw_handle >> '(' >> block_rule >> ',' >> (inline_impl % ',') >> ')')[set_handle_expr{}] |
    (kw_handle >> ast_ident >> '{' >> +handler_func >> '}')[set_handle_decl{}];
BOOST_SPIRIT_DEFINE(handle_rule)

// Standalone anonymous implementation block: Name { fnc method() { } }
// Wrapped in handle_expression with empty body for variant compatibility (Task 57)
struct set_anon_impl {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& impl = x3::_attr(ctx);
        x3::_val(ctx).body = block_expression{};
        x3::_val(ctx).handlers.push_back(impl);
    }
};

auto const anon_impl_rule_def =
    inline_impl[set_anon_impl{}];
BOOST_SPIRIT_DEFINE(anon_impl_rule)

// Intersection type anonymous block: (Trait1 & Trait2) { fnc ... }
struct set_intersection_anon_impl {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        auto& trait_names = fusion::at_c<0>(attr);
        auto& methods = fusion::at_c<1>(attr);
        x3::_val(ctx).body = block_expression{};
        // Create one inline_trait_impl per trait, all sharing the same methods
        for (auto& name : trait_names) {
            inline_trait_impl impl;
            impl.trait_name = name;
            impl.methods = methods;
            x3::_val(ctx).handlers.push_back(impl);
        }
    }
};

auto const intersect_impl_def =
    ('(' >> (ast_ident % '&') >> ')' >> '{' >> +handler_func >> '}')[set_intersection_anon_impl{}];
BOOST_SPIRIT_DEFINE(intersect_impl)

// ============================================================================
// Assertion: assert condition, "message"
// ============================================================================

// ============================================================================
// Test block: test "description" [forall x: int, y: int] { body }
// ============================================================================

auto const kw_test   = x3::lexeme[ x3::lit("test")   >> !ident_body ];
auto const kw_forall = x3::lexeme[ x3::lit("forall") >> !ident_body ];
auto const kw_assume = x3::lexeme[ x3::lit("assume") >> !ident_body ];

struct set_test_block {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        auto& desc = fusion::at_c<0>(attr);
        x3::_val(ctx).has_description = true;
        x3::_val(ctx).description = desc.value;
        auto& maybe_forall = fusion::at_c<1>(attr);
        if (maybe_forall) {
            x3::_val(ctx).is_property_test = true;
            for (auto& v : *maybe_forall) {
                x3::_val(ctx).forall_variables.push_back(v.name);
            }
        }
        x3::_val(ctx).body = fusion::at_c<2>(attr);
    }
};

auto const forall_var = x3::rule<class fv_tag, function_parameter>{} =
    (ast_ident >> ':' >> type_ann)[set_func_param{}];

auto const test_block_rule = x3::rule<class tb_tag, test_block>{} =
    (kw_test >> string_lit >> -(kw_forall >> (forall_var % ',')) >> block_rule
    )[set_test_block{}];

// ============================================================================
// Old expression: old(expr) for postconditions
// ============================================================================

auto const kw_old = x3::lexeme[ x3::lit("old") >> !ident_body ];

auto const old_rule_def =
    kw_old >> '(' >> expression_rule >> ')';
BOOST_SPIRIT_DEFINE(old_rule)

// ============================================================================
// Spread expression: ...expr (inside array literals)
// ============================================================================

auto const spread_expr = x3::rule<class se_tag, spread_expression>{} =
    "..." >> expression_rule;

// ============================================================================
// Tuple destructuring: (val a, var b, _) = expr
// ============================================================================

struct set_destr_binding {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        auto& is_var_variant = fusion::at_c<0>(attr);
        bool is_var = boost::get<bool>(&is_var_variant) ? boost::get<bool>(is_var_variant) : false;
        x3::_val(ctx).is_val = !is_var;
        x3::_val(ctx).name = fusion::at_c<1>(attr);
        x3::_val(ctx).is_placeholder = (x3::_val(ctx).name.name == "_");
    }
};

struct set_placeholder_binding {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        x3::_val(ctx).name.name = "_";
        x3::_val(ctx).is_val = true;
        x3::_val(ctx).is_placeholder = true;
    }
};

auto const placeholder_binding = x3::rule<class pb_tag, destructuring_binding>{} =
    x3::lexeme[x3::lit("_") >> !ident_body][set_placeholder_binding{}];

auto const qual_binding = x3::rule<class qb_tag, destructuring_binding>{} =
    (field_mutability >> ast_ident)[set_destr_binding{}];

auto const destr_binding = placeholder_binding | qual_binding;

struct set_tuple_destr {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        auto& bindings_vec = fusion::at_c<0>(attr);
        for (auto& b : bindings_vec) {
            x3::_val(ctx).bindings.push_back(
                boost::apply_visitor([](const destructuring_binding& db) { return db; }, b));
        }
        x3::_val(ctx).tuple_expr = fusion::at_c<1>(attr);
    }
};

auto const tuple_destr_rule_def =
    ('(' >> (destr_binding % ',') >> ')' >> '=' >> expression_rule)[set_tuple_destr{}];
BOOST_SPIRIT_DEFINE(tuple_destr_rule)

// ============================================================================
// Flow definition: flow Name(initial: State) { state S { ... } }
// ============================================================================

auto const kw_flow  = x3::lexeme[ x3::lit("flow")  >> !ident_body ];
auto const kw_state = x3::lexeme[ x3::lit("state") >> !ident_body ];
auto const kw_on    = x3::lexeme[ x3::lit("on")    >> !ident_body ];
auto const kw_goto  = x3::lexeme[ x3::lit("goto")  >> !ident_body ];
auto const kw_entry = x3::lexeme[ x3::lit("entry")  >> !ident_body ];
auto const kw_exit_kw = x3::lexeme[ x3::lit("exit")  >> !ident_body ];

// Flow transition: on(Event) [when { guard }] => goto Target
struct set_flow_transition {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).event_name = fusion::at_c<0>(attr);
        auto& maybe_guard = fusion::at_c<1>(attr);
        if (maybe_guard) {
            x3::_val(ctx).has_guard = true;
            flow_guard_condition gc;
            gc.condition = *maybe_guard;
            x3::_val(ctx).guard = gc;
        }
        x3::_val(ctx).target_state = fusion::at_c<2>(attr);
    }
};

auto const kw_when = x3::lexeme[ x3::lit("when") >> !ident_body ];

auto const flow_trans = x3::rule<class ft_tag, flow_transition>{} =
    (kw_on >> '(' >> ast_ident >> ')' >>
     -(kw_when >> '{' >> expression_rule >> '}') >>
     "=>" >> kw_goto >> ast_ident
    )[set_flow_transition{}];

// Flow state: state Name { transitions... [entry { } exit { }] }
struct set_flow_state {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).name = fusion::at_c<0>(attr);
        auto& maybe_terminal = fusion::at_c<1>(attr);
        x3::_val(ctx).is_terminal = maybe_terminal;
        x3::_val(ctx).transitions = fusion::at_c<2>(attr);
        auto& maybe_entry = fusion::at_c<3>(attr);
        if (maybe_entry) {
            x3::_val(ctx).has_entry_action = true;
            x3::_val(ctx).entry_action = *maybe_entry;
        }
        auto& maybe_exit = fusion::at_c<4>(attr);
        if (maybe_exit) {
            x3::_val(ctx).has_exit_action = true;
            x3::_val(ctx).exit_action = *maybe_exit;
        }
    }
};

auto const kw_terminal = x3::lexeme[ x3::lit("terminal") >> !ident_body ];

auto const flow_state_rule = x3::rule<class fs_tag, flow_state>{} =
    (kw_state >> ast_ident >> x3::matches[kw_terminal] >> '{' >>
     *flow_trans >>
     -(kw_entry >> block_rule) >>
     -(kw_exit_kw >> block_rule) >>
     '}'
    )[set_flow_state{}];

// Flow definition: flow Name(initial: StateName) { state ... }
struct set_flow_def {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& attr = x3::_attr(ctx);
        x3::_val(ctx).name = fusion::at_c<0>(attr);
        x3::_val(ctx).initial_state = fusion::at_c<1>(attr);
        x3::_val(ctx).states = fusion::at_c<2>(attr);
    }
};

auto const flow_def = x3::rule<class fldef_tag, flow_definition>{} =
    (kw_flow >> ast_ident >> '(' >> ast_ident >> ')' >> '{' >>
     *flow_state_rule >> '}'
    )[set_flow_def{}];

// ============================================================================
// Top-level
// ============================================================================

// ============================================================================
// Top-level parsing: two-phase approach
// Phase 1: Parse type_definitions separately
// Phase 2: Parse regular expressions
// Convert type_definitions to backward-compatible nodes in parse()
// ============================================================================

// Skip annotations at top level — produce empty identifier (filtered in parse())
struct make_empty_expr {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        x3::_val(ctx) = expression(identifier{""});
    }
};

auto const toplevel_annotation = x3::rule<class tla_tag, expression>{} =
    ('@' >> x3::omit[x3::lexeme[ ident_start >> *ident_body ]] >> x3::omit[-('(' >> *(x3::char_ - ')') >> ')')]
    )[make_empty_expr{}];

auto const regular_toplevel =
    imp_decl | type_arrow_decl | typealias_decl | newtype_decl |
    effect_def | extension_def |
    flow_def | test_block_rule | namespace_def |
    func_def | func_sig | opr_def | val_decl | var_decl | field_assign_stmt | assign_stmt | toplevel_annotation | expression_rule;

// A toplevel item is either a type definition or a regular expression.
// We try type definitions first (meta_type_def, instance_type_def),
// then fall back to regular expressions.
// To avoid variant issues, we convert type_definitions inline via semantic action.

struct convert_meta_type_def {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& td = x3::_attr(ctx);
        // Meta-definition: type class { ... } → struct_definition with name
        struct_definition sd;
        sd.name = td.name;
        sd.type_parameters = td.type_parameters;
        sd.fields = td.fields;
        x3::_val(ctx) = expression(sd);
    }
};

struct convert_instance_type_def {
    template<typename Ctx> void operator()(Ctx& ctx) const {
        auto& td = x3::_attr(ctx);
        std::string k = td.kind.name;
        if (k == "enum") {
            enum_definition ed;
            ed.name = td.name;
            ed.type_parameters = td.type_parameters;
            ed.variants = td.variants;
            x3::_val(ctx) = expression(ed);
        } else if (k == "class") {
            class_definition cd;
            cd.name = td.name;
            cd.type_parameters = td.type_parameters;
            cd.fields = td.fields;
            x3::_val(ctx) = expression(cd);
        } else {
            struct_definition sd;
            sd.name = td.name;
            sd.type_parameters = td.type_parameters;
            sd.fields = td.fields;
            x3::_val(ctx) = expression(sd);
        }
    }
};

auto const meta_as_expr = x3::rule<class mae_tag, expression>{} =
    meta_type_def[convert_meta_type_def{}];

// Only try instance_type_def when we see ident ident pattern (lookahead)
auto const two_idents_ahead = &(x3::lexeme[ident_start >> *ident_body] >> x3::lexeme[ident_start >> *ident_body]);

auto const instance_as_expr = x3::rule<class iae_tag, expression>{} =
    (two_idents_ahead >> instance_type_def)[convert_instance_type_def{}];

auto const toplevel_item_def =
    (meta_as_expr | instance_as_expr | regular_toplevel);
BOOST_SPIRIT_DEFINE(toplevel_item)

// ============================================================================
// Entry point
// ============================================================================

bool parse(const std::string& source, std::vector<expression>& result, std::string& error) {
    auto iter = source.begin();
    auto end  = source.end();

    bool ok = x3::phrase_parse(iter, end, *toplevel_item, skipper, result);

    if (!ok || iter != end) {
        size_t pos = std::distance(source.begin(), iter);
        size_t line = 1, col = 1;
        for (size_t i = 0; i < pos && i < source.size(); ++i) {
            if (source[i] == '\n') { ++line; col = 1; } else { ++col; }
        }
        error = "Parse error at line " + std::to_string(line) + ":" + std::to_string(col);
        if (iter != end) {
            auto snippet_end = iter + std::min<ptrdiff_t>(30, end - iter);
            error += " near '" + std::string(iter, snippet_end) + "'";
        }
        return false;
    }

    return true;
}

} // namespace meld::parser::grammar
