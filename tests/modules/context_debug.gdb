set pagination off
set confirm off
set debuginfod enabled off
python
import gdb
try:
    gdb.Breakpoint("_std_io_cst_printf")
    gdb.execute("run")
    gdb.execute("up")
    expected = {"root.root_value": 17, "middle.module_tail": 38,
                "leaf.leaf_guard": 66, "box.child.leaf_tail": 70,
                "box.marker": 97, "aliased.leaf_guard": 94}
    for expression, value in expected.items():
        actual = int(gdb.parse_and_eval(expression))
        assert actual == value, (expression, actual, value)
    for variable, names in [("root_kind", ["ROOT_ONLY"]),
                            ("module_kind", ["MODULE_ZERO", "MODULE_ONE"])]:
        actual = [field.name for field in gdb.parse_and_eval(variable).type.fields()]
        assert actual == names, (variable, actual, names)
except Exception as error:
    print("FAIL debug module context:", error)
    gdb.execute("quit 1")
print("debug module context: field and enum identities passed")
gdb.execute("quit 0")
end
