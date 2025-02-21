import os
import sys
import shutil
from enum import Enum

if len(sys.argv) != 4:
    raise ValueError(
        "Provide in that order: ReturnOfModdingBase src folder path, your lib src folder path, and the folder path where the doc will end up."
    )

rom_base_src_folder = sys.argv[1]

src_folders = [rom_base_src_folder, sys.argv[2]]

output_doc_folder_path = sys.argv[3]

lua_api_namespace = ""
lua_api_comment_identifier = "lua api"
lua_api_comment_separator = ":"

tables = {}
classes = {}
functions = {}


class DocKind(Enum):
    Table = "table"
    Class = "class"
    Field = "field"
    Constructor = "constructor"
    Function = "function"
    Namespace = "namespace"


class Table:
    def __init__(self, name, fields, functions, description):
        self.name = name.strip()
        self.fields = fields
        self.functions = functions
        self.description = description

    def print_definition(self):
        global lua_api_namespace

        s = f"---@meta {self.name}\n\n"

        if len(self.description) > 0:
            description_lua = self.description.replace("\n", "\n--")
            s += f"-- {description_lua}\n"

        if len(lua_api_namespace) > 0 and "Global Table" not in self.name:
            s += f"---@class (exact) {lua_api_namespace}.{self.name}"
        else:
            s += f"---@class (exact) {self.name}"

        s += "\n"

        if len(self.fields) > 0:
            self.check_for_duplicate_fields_names()

            for field in self.fields:
                s += field.print_definition()

        s += "\n"

        if len(self.functions) > 0:
            for func in self.functions:
                s += func.print_definition(f"{self.name}.")

            s += "\n"

        if len(lua_api_namespace) > 0:
            s = s.replace("{LUA_API_NAMESPACE}", lua_api_namespace)
        else:
            s = s.replace("{LUA_API_NAMESPACE}.", lua_api_namespace)

        return s

    def __str__(self):
        global lua_api_namespace

        if len(lua_api_namespace) > 0 and "Global Table" not in self.name:
            s = f"# Table: {lua_api_namespace}.{self.name}\n"
        else:
            s = f"# Table: {self.name}\n"
        s += "\n"

        if len(self.description) > 0:
            s += f"{self.description}\n"
            s += "\n"

        if len(self.fields) > 0:
            s += f"## Fields ({len(self.fields)})\n"
            s += "\n"

            self.check_for_duplicate_fields_names()

            for field in self.fields:
                s += field.print_markdown()

        if len(self.functions) > 0:
            s += f"## Functions ({len(self.functions)})\n"
            s += "\n"

            for func in self.functions:
                s += func.print_markdown(f"{self.name}.")

            s += "\n"

        if len(lua_api_namespace) > 0:
            s = s.replace("{LUA_API_NAMESPACE}", lua_api_namespace)
        else:
            s = s.replace("{LUA_API_NAMESPACE}.", lua_api_namespace)

        return s

    def check_for_duplicate_fields_names(self):
        seen = set()
        duplicates = [x for x in self.fields if x.name in seen or seen.add(x.name)]
        if len(duplicates) > 0:
            print("Error while building lua doc. Duplicate field names:")
            for dup in duplicates:
                print(dup)
            exit(1)


class Class:
    def __init__(self, name, inheritance, fields, constructors, functions, description):
        self.name = name.strip()
        self.inheritance = inheritance
        self.fields = fields
        self.constructors = constructors
        self.functions = functions
        self.description = description

    def print_definition(self):
        global lua_api_namespace

        s = f"---@meta {self.name}\n\n"

        if len(self.description) > 0:
            description_lua = self.description.replace("\n", "\n--")
            s += f"-- {description_lua}\n"

        if len(lua_api_namespace) > 0:
            s += f"---@class (exact) {lua_api_namespace}.{self.name}"
        else:
            s += f"---@class (exact) {self.name}"

        if len(self.inheritance) > 0:
            s += ": "
            inherited_class_names = ", ".join(self.inheritance)
            s += f"{inherited_class_names}"

        s += "\n"

        if len(self.fields) > 0:
            self.check_for_duplicate_fields_names()

            for field in self.fields:
                s += field.print_definition()
        else:
            s += "\n"

        s += "\n"

        if len(self.constructors) > 0:
            for ctor in self.constructors:
                s += ctor.print_definition()
        else:
            s += "\n"

        s += "\n"

        if len(self.functions) > 0:
            for func in self.functions:
                s += func.print_definition(f"{self.name}:")

            s += "\n"

        if len(lua_api_namespace) > 0:
            s = s.replace("{LUA_API_NAMESPACE}", lua_api_namespace)
        else:
            s = s.replace("{LUA_API_NAMESPACE}.", lua_api_namespace)

        return s

    def __str__(self):
        global lua_api_namespace

        if len(lua_api_namespace) > 0:
            s = f"# Class: {lua_api_namespace}.{self.name}\n"
        else:
            s = f"# Class: {self.name}\n"
        s += "\n"

        if len(self.inheritance) > 0:
            inherited_class_names = ", ".join(self.inheritance)
            s += f"## Inherit from {len(self.inheritance)} class: {inherited_class_names}\n"
            s += "\n"

        if len(self.description) > 0:
            s += f"{self.description}\n"
            s += "\n"

        if len(self.fields) > 0:
            s += f"## Fields ({len(self.fields)})\n"
            s += "\n"

            self.check_for_duplicate_fields_names()

            for field in self.fields:
                s += field.print_markdown()

        if len(self.constructors) > 0:
            s += f"## Constructors ({len(self.constructors)})\n"
            s += "\n"

            for ctor in self.constructors:
                s += ctor.print_markdown()

        if len(self.functions) > 0:
            s += f"## Functions ({len(self.functions)})\n"
            s += "\n"

            for func in self.functions:
                s += func.print_markdown(f"{self.name}:")

            s += "\n"

        if len(lua_api_namespace) > 0:
            s = s.replace("{LUA_API_NAMESPACE}", lua_api_namespace)
        else:
            s = s.replace("{LUA_API_NAMESPACE}.", lua_api_namespace)

        return s

    def check_for_duplicate_fields_names(self):
        seen = set()
        duplicates = [x for x in self.fields if x.name in seen or seen.add(x.name)]
        if len(duplicates) > 0:
            print("Error while building lua doc. Duplicate field names:")
            for dup in duplicates:
                print(dup)
            exit(1)


class Field:
    def __init__(self, name, type_, description):
        self.name = name.strip()
        self.type_ = type_.strip()
        self.description = description

    def __str__(self):
        s = f"Field: {self.name}\n"
        s += f"Type: {self.type_}\n"
        s += f"Description: {self.description.strip()}\n"
        return s

    def print_definition(self):
        s = f"---@field {self.name}"

        if self.type_ is not None and len(self.type_) > 0:
            s += f" {self.type_}"

        if len(self.description) > 0:
            description_lua = self.description.replace(".\n", ".").replace("\n", "")
            s += f" # {description_lua}"

        s += "\n"

        return s

    def print_markdown(self):
        s = ""

        s += f"### `{self.name}`\n"
        s += "\n"

        if len(self.description) > 0:
            s += f"{self.description}\n"
            s += "\n"

        if self.type_ is not None and len(self.type_) > 0:
            s += f"- Type: `{self.type_}`\n"

        s += "\n"

        return s


class Constructor:
    def __init__(self, parent, parameters, description):
        self.parent = parent
        self.parameters = parameters
        self.description = description

    def print_definition(self):
        s = ""

        parameters_str = ", ".join(p.name for p in self.parameters)

        if len(self.description) > 0:
            description_lua = self.description.replace("\n", "\n--")
            s += f"-- {description_lua}\n"

        if len(self.parameters) > 0:
            for param in self.parameters:
                s += f"---@param {param.name} {param.type_}"
                if len(param.description) > 0:
                    description_lua = param.description.replace("\n", "\n--")
                    s += f" {description_lua}"
                s += "\n"

        s += f"---@return {self.parent.name}\n"
        s += f"function {self.parent.name}:new({parameters_str}) end\n"

        return s

    def print_markdown(self):
        s = ""

        parameters_str = ", ".join(p.name for p in self.parameters)

        s += f"### `new({parameters_str})`\n"
        s += "\n"

        if len(self.description) > 0:
            s += f"{self.description}\n"
            s += "\n"

        if len(self.parameters) > 0:
            s += f"- **Parameters:**\n"
            for param in self.parameters:
                s += f"  - `{param.name}` ({param.type_})"
                if len(param.description) > 0:
                    s += f": {param.description}\n"
                else:
                    s += f"\n"
            s += "\n"

        s += f"**Example Usage:**\n"
        s += "```lua\n"

        s += f"myInstance = {self.parent.name}:new({parameters_str})\n"

        s += "```\n"
        s += "\n"

        return s


class Parameter:
    def __init__(self, name, type_, description):
        self.name = name.strip()
        self.type_ = type_.strip()
        self.description = description

    def __str__(self):
        s = f"Parameter: {self.name}\n"
        s += f"Type: {self.type_}\n"
        s += f"Description: {self.description.strip()}\n"
        return s


class Function:
    def __init__(
        self, name, parent, parameters, return_type, return_description, description
    ):
        self.name = name.strip()
        self.parent = parent
        self.parameters = parameters
        self.return_type = return_type
        self.return_description = return_description
        self.description = description

    def __str__(self):
        s = f"Function: {self.name}\n"

        type_name = str(type(self.parent)).split(".")[1][:-2]
        s += f"Parent: {self.parent.name} ({type_name})\n"

        s += f"Parameters: {len(self.parameters)}\n"
        i = 1
        for param in self.parameters:
            s += f"Parameter {i}\n"
            s += str(param) + "\n"
            i += 1

        s += f"Return Type: {self.return_type}\n"
        s += f"Return Description: {self.return_description}\n"

        s += f"Description: {self.description}\n"
        return s

    def print_definition(self, prefix):
        s = ""

        parameters_str = ", ".join(p.name for p in self.parameters)

        if len(self.description) > 0:
            if "Global Table" not in prefix:
                if len(lua_api_namespace) > 0:
                    a = lua_api_namespace + "." + prefix + self.name
                else:
                    a = prefix + self.name

                description_lua = self.description.replace("\n", "\n--")
                s += f"-- {description_lua.replace(prefix + self.name, a)}\n"
            else:
                s += f"-- {description_lua}\n"

        if len(self.parameters) > 0:
            for param in self.parameters:
                s += f"---@param {param.name} {param.type_}"
                if len(param.description) > 0:
                    s += f" {param.description}"
                s += "\n"

        if self.return_type is not None and len(self.return_type) > 0:
            s += f"---@return {self.return_type} # {self.return_description}"

            s += "\n"

        table_or_class_sep = "." if isinstance(self.parent, Table) else ":"
        s += f"function {self.parent.name}{table_or_class_sep}{self.name}({parameters_str}) end\n"

        s += "\n"

        return s

    def print_markdown(self, prefix):
        s = ""

        parameters_str = ", ".join(p.name for p in self.parameters)

        s += f"### `{self.name}({parameters_str})`\n"
        s += "\n"

        if len(self.description) > 0:
            if "Global Table" not in prefix:
                if len(lua_api_namespace) > 0:
                    a = lua_api_namespace + "." + prefix + self.name
                else:
                    a = prefix + self.name
                s += f"{self.description.replace(prefix + self.name, a)}\n"
            else:
                s += f"{self.description}\n"
            s += "\n"

        if len(self.parameters) > 0:
            s += f"- **Parameters:**\n"
            for param in self.parameters:
                s += f"  - `{param.name}` ({param.type_})"
                if len(param.description) > 0:
                    s += f": {param.description}\n"
                else:
                    s += f"\n"
            s += "\n"

        if self.return_type is not None and len(self.return_type) > 0:
            s += f"- **Returns:**\n"
            s += f"  - `{self.return_type}`: {self.return_description}\n"

            s += "\n"

        s += f"**Example Usage:**\n"
        s += "```lua\n"
        if self.return_type is not None and len(self.return_type) > 0:
            s += self.return_type + " = "

        if "Global Table" in prefix:
            prefix = ""
        else:
            if len(lua_api_namespace) > 0:
                prefix = lua_api_namespace + "." + prefix
            else:
                prefix = prefix

        s += f"{prefix}{self.name}({parameters_str})\n"

        s += "```\n"
        s += "\n"

        return s


def make_table(table_name):
    if table_name not in tables:
        tables[table_name] = Table(table_name, [], [], "")
    cur_table = tables[table_name]
    return cur_table


def make_class(class_name):
    if class_name not in classes:
        classes[class_name] = Class(class_name, [], [], [], [], "")
    cur_class = classes[class_name]
    return cur_class


def is_comment_a_lua_api_doc_comment(text_lower):
    return (
        lua_api_comment_identifier in text_lower
        and lua_api_comment_separator in text_lower
        and "//" in text_lower
    )


def parse_lua_api_doc(folder_paths):
    for folder_path in folder_paths:
        for root, dirs, files in os.walk(folder_path):
            for file_name in files:
                if os.path.splitext(file_name)[1].startswith((".c", ".h")):
                    file_path = os.path.join(root, file_name)
                    with open(file_path, "r") as file:
                        doc_kind = None
                        cur_table = None
                        cur_class = None
                        cur_function = None
                        cur_field = None
                        cur_constructor = None

                        for line in file:
                            line = line.strip()
                            line_lower = line.lower()
                            if is_comment_a_lua_api_doc_comment(line_lower):
                                doc_kind = DocKind(
                                    line.split(lua_api_comment_separator, 1)[1]
                                    .strip()
                                    .lower()
                                )
                                continue

                            if doc_kind is not None and "//" in line:
                                match doc_kind:
                                    case DocKind.Table:
                                        cur_table = parse_table_doc(
                                            cur_table, line, line_lower
                                        )
                                    case DocKind.Class:
                                        cur_class = parse_class_doc(
                                            cur_class, line, line_lower
                                        )
                                    case DocKind.Function:
                                        (
                                            cur_function,
                                            cur_table,
                                            cur_class,
                                        ) = parse_function_doc(
                                            cur_function,
                                            cur_table,
                                            cur_class,
                                            line,
                                            line_lower,
                                        )
                                    case DocKind.Field:
                                        (cur_field, cur_table, cur_class) = (
                                            parse_field_doc(
                                                cur_field,
                                                cur_table,
                                                cur_class,
                                                line,
                                                line_lower,
                                            )
                                        )
                                    case DocKind.Constructor:
                                        (
                                            cur_constructor,
                                            cur_class,
                                        ) = parse_constructor_doc(
                                            cur_constructor,
                                            cur_class,
                                            line,
                                            line_lower,
                                        )
                                    case DocKind.Namespace:
                                        parse_namespace(
                                            line,
                                            line_lower,
                                        )
                                    case _:
                                        # print("unsupported doc kind: " + str(doc_kind))
                                        pass
                            else:
                                doc_kind = None


def parse_table_doc(cur_table, line, line_lower):
    if is_lua_doc_comment_startswith(line_lower, "name"):
        table_name = line.split(lua_api_comment_separator, 1)[1].strip()
        cur_table = make_table(table_name)
    else:
        if len(cur_table.description) != 0:
            cur_table.description += "\n"
        cur_table.description += sanitize_description(line)

    return cur_table


def parse_class_doc(cur_class, line, line_lower):
    if is_lua_doc_comment_startswith(line_lower, "name"):
        class_name = line.split(lua_api_comment_separator, 1)[1].strip()
        cur_class = make_class(class_name)
    elif is_lua_doc_comment_startswith(line_lower, "inherit"):
        inherited_class_name = line.split(lua_api_comment_separator, 1)[1].strip()
        cur_class.inheritance.append(inherited_class_name)
    else:
        if len(cur_class.description) != 0:
            cur_class.description += "\n"
        cur_class.description += sanitize_description(line)

    return cur_class


def parse_function_doc(cur_function, cur_table, cur_class, line, line_lower):
    if (
        is_lua_doc_comment_startswith(line_lower, "table")
        and lua_api_comment_separator in line_lower
    ):
        table_name = line.split(lua_api_comment_separator, 1)[1].strip()
        cur_table = make_table(table_name)

        cur_function = Function("Didnt get name yet", cur_table, [], None, "", "")
        cur_table.functions.append(cur_function)
    elif (
        is_lua_doc_comment_startswith(line_lower, "class")
        and lua_api_comment_separator in line_lower
    ):
        class_name = line.split(lua_api_comment_separator, 1)[1].strip()
        cur_class = make_class(class_name)

        cur_function = Function("Didnt get name yet", cur_class, [], None, "", "")
        cur_class.functions.append(cur_function)
    elif (
        is_lua_doc_comment_startswith(line_lower, "name")
        and lua_api_comment_separator in line_lower
    ):
        function_name = line.split(lua_api_comment_separator, 1)[1].strip()
        cur_function.name = function_name

        if function_name not in functions:
            functions[function_name] = cur_function
    elif (
        is_lua_doc_comment_startswith(line_lower, "param")
        and lua_api_comment_separator in line_lower
    ):
        parameter = make_parameter_from_doc_line(line)
        cur_function.parameters.append(parameter)
    elif (
        is_lua_doc_comment_startswith(line_lower, "return")
        and lua_api_comment_separator in line_lower
    ):
        return_info = line.split(lua_api_comment_separator, 2)
        try:
            cur_function.return_type = return_info[1].strip()
            cur_function.return_description = return_info[2].strip()
        except IndexError:
            pass
    else:
        if len(cur_function.description) != 0:
            cur_function.description += "\n"
        cur_function.description += sanitize_description(line)

    return cur_function, cur_table, cur_class


def parse_field_doc(cur_field, cur_table, cur_class, line, line_lower):
    if (
        is_lua_doc_comment_startswith(line_lower, "table")
        and lua_api_comment_separator in line_lower
    ):
        table_name = line.split(lua_api_comment_separator, 1)[1].strip()
        cur_table = make_table(table_name)

        cur_field = Field("Didnt get name yet", "", "")
        cur_table.fields.append(cur_field)
    elif (
        is_lua_doc_comment_startswith(line_lower, "class")
        and lua_api_comment_separator in line_lower
    ):
        class_name = line.split(lua_api_comment_separator, 1)[1].strip()
        cur_class = make_class(class_name)

        cur_field = Field("Didnt get name yet", "", "")
        cur_class.fields.append(cur_field)
    elif (
        is_lua_doc_comment_startswith(line_lower, "field")
        and lua_api_comment_separator in line_lower
    ):
        field_info = line.split(lua_api_comment_separator, 2)
        cur_field.name = field_info[1].strip()
        cur_field.type_ = field_info[2].strip()
    else:
        if len(cur_field.description) != 0:
            cur_field.description += "\n"

        if line.startswith("// "):
            line = line[3:]
        cur_field.description += sanitize_description(line)

    return cur_field, cur_table, cur_class


def parse_namespace(line, line_lower):
    global lua_api_namespace
    if (
        is_lua_doc_comment_startswith(line_lower, "name")
        and lua_api_comment_separator in line_lower
    ):
        lua_api_namespace = line.split(lua_api_comment_separator, 1)[1].strip()


def parse_constructor_doc(cur_constructor, cur_class, line, line_lower):
    if (
        is_lua_doc_comment_startswith(line_lower, "class")
        and lua_api_comment_separator in line_lower
    ):
        class_name = line.split(lua_api_comment_separator, 1)[1].strip()
        cur_class = make_class(class_name)

        cur_constructor = Constructor(cur_class, [], "")
        cur_class.constructors.append(cur_constructor)
    elif (
        is_lua_doc_comment_startswith(line_lower, "param")
        and lua_api_comment_separator in line_lower
    ):
        parameter = make_parameter_from_doc_line(line)
        cur_constructor.parameters.append(parameter)
    else:
        if len(cur_constructor.description) != 0:
            cur_constructor.description += "\n"
        cur_constructor.description += sanitize_description(line)

    return cur_constructor, cur_class


def make_parameter_from_doc_line(line):
    param_info = line.split(lua_api_comment_separator, 3)[1:]
    param_name = param_type = param_desc = ""

    try:
        param_name = param_info[0].strip()
        param_type = param_info[1].strip()
        param_desc = param_info[2].strip()
    except IndexError:
        pass

    return Parameter(param_name, param_type, param_desc)


def sanitize_description(line):
    if line.startswith("// ") and line[3] != " ":
        line = line[3:]
    if line.startswith("//"):
        line = line[2:]
    return line.rstrip()


def is_lua_doc_comment_startswith(line_lower, starts_with_text):
    return line_lower.replace("//", "").strip().startswith(starts_with_text)


def append_lua_api_namespace_to_str(thing):
    if len(lua_api_namespace) > 0:
        thing = lua_api_namespace + "." + thing
    return thing


parse_lua_api_doc(src_folders)

output_tables_folder_path = os.path.join(output_doc_folder_path, "tables")

try:
    os.makedirs(output_tables_folder_path)
except:
    pass


for table_name, table in tables.items():
    if "Global Table" not in table_name:
        table_name = append_lua_api_namespace_to_str(table_name)

    file_name = os.path.join(output_tables_folder_path, f"{table_name}.md")
    if os.path.exists(file_name):
        os.remove(file_name)
    f = open(file_name, "ba")
    f.write(bytes(str(table), "UTF8"))
    f.close()
print("Wrote tables")

for table_name, table in tables.items():
    if "Global Table" not in table_name:
        table_name = append_lua_api_namespace_to_str(table_name)

    file_name = os.path.join(output_tables_folder_path, f"{table_name}.lua")
    if os.path.exists(file_name):
        os.remove(file_name)
    f = open(file_name, "ba")
    f.write(bytes(str(table.print_definition()), "UTF8"))
    f.close()
print("Wrote table definitions")

output_classes_folder_path = os.path.join(output_doc_folder_path, "classes")

try:
    os.makedirs(output_classes_folder_path)
except:
    pass

for class_name, class_ in classes.items():
    class_name = append_lua_api_namespace_to_str(class_name)

    file_name = os.path.join(output_classes_folder_path, f"{class_name}.md")
    if os.path.exists(file_name):
        os.remove(file_name)
    f = open(file_name, "ba")
    f.write(bytes(str(class_), "UTF8"))
    f.close()
print("Wrote classes")

for class_name, class_ in classes.items():
    class_name = append_lua_api_namespace_to_str(class_name)

    file_name = os.path.join(output_classes_folder_path, f"{class_name}.lua")
    if os.path.exists(file_name):
        os.remove(file_name)
    f = open(file_name, "ba")
    f.write(bytes(str(class_.print_definition()), "UTF8"))
    f.close()
print("Wrote class definitions")


def copy_folder_contents(source_folder, destination_folder):
    # Make sure source folder exists
    if not os.path.exists(source_folder):
        # print(f"Source folder '{source_folder}' does not exist. Not copying folder. (This is most likely ok)")
        return
    else:
        print(f"Copying '{source_folder}' to '{destination_folder}'")

    # Make sure destination folder exists
    if not os.path.exists(destination_folder):
        os.makedirs(destination_folder)

    # Iterate over the files and subfolders in the source folder
    for item in os.listdir(source_folder):
        source_item = os.path.join(source_folder, item)
        destination_item = os.path.join(destination_folder, item)

        # If it's a file, copy it to the destination folder
        if os.path.isfile(source_item):
            shutil.copy2(source_item, destination_item)
        # If it's a subfolder, recursively copy its contents
        elif os.path.isdir(source_item):
            shutil.copytree(source_item, destination_item, symlinks=True)


copy_folder_contents(
    os.path.join(rom_base_src_folder, "..", "docs", "lua", "classes"),
    os.path.join(output_doc_folder_path, "classes"),
)
copy_folder_contents(
    os.path.join(rom_base_src_folder, "..", "docs", "lua", "tables"),
    os.path.join(output_doc_folder_path, "tables"),
)
