"""
LLDB Python data formatters for Meld kernel types.

Bundled copy of meld-lang/tools/lldb/meld_formatters.py for the VS Code
extension package. Since VS Code extensions cannot rely on symlinks, this
file is included directly in the extension's formatters/ directory.

The debugAdapterFactory.ts resolveCompiled() method injects:
  command script import ${extensionPath}/formatters/meld_formatters.py
as an LLDB initCommand so kernel types display correctly in the Variables panel.

Provides human-readable display of kernel::Integer, kernel::String,
kernel::Boolean, kernel::Vec, kernel::Optional, kernel::Function,
and kernel::Cons when debugging AOT-compiled Meld programs.

Requirements: 12C.1, 12C.2, 12C.3, 12C.4, 12C.5, 19.7
"""

import lldb


# ─── kernel::Integer ─────────────────────────────────────────────────

class IntegerSynthProvider:
    """Display kernel::Integer as its numeric value."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj

    def update(self):
        pass

    def num_children(self):
        return 0

    def has_children(self):
        return False


def integer_summary(valobj, internal_dict):
    """Summary for kernel::Integer — shows the numeric value."""
    value_member = valobj.GetChildMemberWithName("value_")
    if not value_member.IsValid():
        value_member = valobj.GetChildMemberWithName("value")
    if value_member.IsValid():
        return str(value_member.GetValueAsSigned())
    return "<unknown Integer>"


# ─── kernel::Float ───────────────────────────────────────────────────

def float_summary(valobj, internal_dict):
    """Summary for kernel::Float — shows the floating-point value."""
    value_member = valobj.GetChildMemberWithName("value_")
    if not value_member.IsValid():
        value_member = valobj.GetChildMemberWithName("value")
    if value_member.IsValid():
        err = lldb.SBError()
        val = value_member.GetData().GetDouble(err, 0)
        if err.Success():
            return str(val)
    return "<unknown Float>"


# ─── kernel::Boolean ─────────────────────────────────────────────────

def boolean_summary(valobj, internal_dict):
    """Summary for kernel::Boolean — shows true/false."""
    value_member = valobj.GetChildMemberWithName("value_")
    if not value_member.IsValid():
        value_member = valobj.GetChildMemberWithName("value")
    if value_member.IsValid():
        return "true" if value_member.GetValueAsUnsigned() != 0 else "false"
    return "<unknown Boolean>"


# ─── kernel::String ──────────────────────────────────────────────────

def string_summary(valobj, internal_dict):
    """Summary for kernel::String — shows the string content directly."""
    for field_name in ("value_", "value", "data_", "str_"):
        member = valobj.GetChildMemberWithName(field_name)
        if member.IsValid():
            summary = member.GetSummary()
            if summary:
                return summary
            data_member = member.GetChildMemberWithName("__r_")
            if data_member.IsValid():
                return member.GetSummary() or "<std::string>"
    return "<unknown String>"


# ─── kernel::Vec ─────────────────────────────────────────────────────

class VecSynthProvider:
    """Synthetic children provider for kernel::Vec.

    Displays as: vec[T] { elem0, elem1, ... } with element count.
    """

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.elements = []

    def update(self):
        self.elements = []
        for field_name in ("elements_", "data_", "items_", "vec_"):
            member = self.valobj.GetChildMemberWithName(field_name)
            if member.IsValid():
                count = member.GetNumChildren()
                for i in range(min(count, 256)):
                    child = member.GetChildAtIndex(i)
                    if child.IsValid():
                        self.elements.append(child)
                break

    def num_children(self):
        return len(self.elements)

    def get_child_at_index(self, index):
        if 0 <= index < len(self.elements):
            return self.elements[index]
        return None

    def get_child_index(self, name):
        try:
            return int(name.lstrip("[").rstrip("]"))
        except ValueError:
            return -1

    def has_children(self):
        return True


def vec_summary(valobj, internal_dict):
    """Summary for kernel::Vec — shows element count and preview."""
    for field_name in ("elements_", "data_", "items_", "vec_"):
        member = valobj.GetChildMemberWithName(field_name)
        if member.IsValid():
            count = member.GetNumChildren()
            if count == 0:
                return "vec[] { }"
            previews = []
            for i in range(min(count, 5)):
                child = member.GetChildAtIndex(i)
                if child.IsValid():
                    s = child.GetSummary() or child.GetValue() or "?"
                    previews.append(s)
            suffix = ", ..." if count > 5 else ""
            return "vec[{}] {{ {} }}".format(count, ", ".join(previews) + suffix)
    return "vec[?] { ... }"


# ─── kernel::Optional ────────────────────────────────────────────────

def optional_summary(valobj, internal_dict):
    """Summary for kernel::Optional — shows some(value) or nil."""
    for flag_name in ("has_value_", "engaged_", "has_value"):
        flag = valobj.GetChildMemberWithName(flag_name)
        if flag.IsValid():
            if flag.GetValueAsUnsigned() == 0:
                return "nil"
            for val_name in ("value_", "val_", "storage_"):
                val = valobj.GetChildMemberWithName(val_name)
                if val.IsValid():
                    s = val.GetSummary() or val.GetValue() or "?"
                    return "some({})".format(s)
            return "some(?)"
    return "<unknown Optional>"


# ─── kernel::Function ────────────────────────────────────────────────

def function_summary(valobj, internal_dict):
    """Summary for kernel::Function — shows name, param count, closure/native."""
    name_member = valobj.GetChildMemberWithName("name_")
    if not name_member.IsValid():
        name_member = valobj.GetChildMemberWithName("name")

    fn_name = "<anonymous>"
    if name_member.IsValid():
        s = name_member.GetSummary()
        if s:
            fn_name = s.strip('"')

    params = valobj.GetChildMemberWithName("parameters_")
    if not params.IsValid():
        params = valobj.GetChildMemberWithName("params_")
    param_count = params.GetNumChildren() if params.IsValid() else "?"

    is_native = valobj.GetChildMemberWithName("is_native_")
    kind = "native" if (is_native.IsValid() and is_native.GetValueAsUnsigned() != 0) else "closure"

    return "fnc {}({} params) [{}]".format(fn_name, param_count, kind)


# ─── kernel::Cons ────────────────────────────────────────────────────

def cons_summary(valobj, internal_dict):
    """Summary for kernel::Cons — shows cons cell structure."""
    head = valobj.GetChildMemberWithName("head_")
    if not head.IsValid():
        head = valobj.GetChildMemberWithName("car_")
    tail = valobj.GetChildMemberWithName("tail_")
    if not tail.IsValid():
        tail = valobj.GetChildMemberWithName("cdr_")

    head_str = head.GetSummary() or head.GetValue() or "?" if head.IsValid() else "?"
    tail_str = tail.GetSummary() or tail.GetValue() or "?" if tail.IsValid() else "?"

    return "({} . {})".format(head_str, tail_str)


# ─── Registration ────────────────────────────────────────────────────

def __lldb_init_module(debugger, internal_dict):
    """Auto-register all Meld kernel type formatters with LLDB."""

    prefix = "meld::kernel"

    # Summary formatters
    debugger.HandleCommand(
        'type summary add -F meld_formatters.integer_summary "{}::Integer"'.format(prefix))
    debugger.HandleCommand(
        'type summary add -F meld_formatters.float_summary "{}::Float"'.format(prefix))
    debugger.HandleCommand(
        'type summary add -F meld_formatters.boolean_summary "{}::Boolean"'.format(prefix))
    debugger.HandleCommand(
        'type summary add -F meld_formatters.string_summary "{}::String"'.format(prefix))
    debugger.HandleCommand(
        'type summary add -F meld_formatters.vec_summary "{}::Vec"'.format(prefix))
    debugger.HandleCommand(
        'type summary add -F meld_formatters.optional_summary "{}::Optional"'.format(prefix))
    debugger.HandleCommand(
        'type summary add -F meld_formatters.function_summary "{}::Function"'.format(prefix))
    debugger.HandleCommand(
        'type summary add -F meld_formatters.cons_summary "{}::Cons"'.format(prefix))

    # Synthetic children for Vec (expandable in debugger)
    debugger.HandleCommand(
        'type synthetic add -l meld_formatters.VecSynthProvider "{}::Vec"'.format(prefix))

    print("[meld] Registered LLDB formatters for kernel types.")
