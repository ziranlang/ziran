/*
 * zir_py_runtime.c - the Python support code generated programs call.
 *
 * Each item is one top-level Python definition. The lowerer writes an item
 * only when the program, or another item it keeps, names it.
 */
#include "zir_py_runtime.h"

#include <stddef.h>

const PyRuntimeItem py_runtime_items[] = {
    {"_s8", "",
     "def _s8(value):\n"
     "    return ((value + 0x80) & 0xFF) - 0x80\n"},
    {"_s16", "",
     "def _s16(value):\n"
     "    return ((value + 0x8000) & 0xFFFF) - 0x8000\n"},
    {"_s32", "",
     "def _s32(value):\n"
     "    return ((value + 0x80000000) & 0xFFFFFFFF) - 0x80000000\n"},
    {"_s64", "",
     "def _s64(value):\n"
     "    return ((value + 0x8000000000000000) & 0xFFFFFFFFFFFFFFFF) - 0x8000000000000000\n"},
    {"_u8", "",
     "def _u8(value):\n"
     "    return value & 0xFF\n"},
    {"_u16", "",
     "def _u16(value):\n"
     "    return value & 0xFFFF\n"},
    {"_u32", "",
     "def _u32(value):\n"
     "    return value & 0xFFFFFFFF\n"},
    {"_u64", "",
     "def _u64(value):\n"
     "    return value & 0xFFFFFFFFFFFFFFFF\n"},
    {"_div", "",
     "def _div(left, right):\n"
     "    \"\"\"Integer division that truncates toward zero, as C does.\"\"\"\n"
     "    quotient = abs(left) // abs(right)\n"
     "    return quotient if (left < 0) == (right < 0) else -quotient\n"},
    {"_rem", "",
     "def _rem(left, right):\n"
     "    \"\"\"The remainder of _div, with the sign of the dividend.\"\"\"\n"
     "    return left - right * _div(left, right)\n"},
    {"_shl", "",
     "def _shl(value, count, width):\n"
     "    if count < 0 or count >= width:\n"
     "        raise ArithmeticError(\"shift count out of range\")\n"
     "    return value << count\n"},
    {"_shr", "",
     "def _shr(value, count, width):\n"
     "    if count < 0 or count >= width:\n"
     "        raise ArithmeticError(\"shift count out of range\")\n"
     "    return value >> count\n"},
    {"_fdiv", "math",
     "def _fdiv(left, right):\n"
     "    \"\"\"IEEE division: a zero divisor gives an infinity or NaN.\"\"\"\n"
     "    if right == 0.0:\n"
     "        if left == 0.0 or left != left:\n"
     "            return math.nan\n"
     "        return math.copysign(math.inf, left) * math.copysign(1.0, right)\n"
     "    return left / right\n"},
    {"_frem", "math",
     "def _frem(left, right):\n"
     "    if right == 0.0:\n"
     "        return math.nan\n"
     "    return math.fmod(left, right)\n"},
    {"_f32", "math struct",
     "def _f32(value):\n"
     "    \"\"\"Round a float to the nearest float32.\"\"\"\n"
     "    try:\n"
     "        return struct.unpack(\"f\", struct.pack(\"f\", value))[0]\n"
     "    except OverflowError:\n"
     "        return math.copysign(math.inf, value)\n"},
    {"_float_text", "decimal",
     "def _float_text(value, single=False):\n"
     "    \"\"\"The shortest decimal that reads back as value, never in exponent form.\"\"\"\n"
     "    if value != value:\n"
     "        return b\"nan\"\n"
     "    if value - value != 0:\n"
     "        return b\"-inf\" if value < 0 else b\"inf\"\n"
     "    text = repr(value)\n"
     "    if single:\n"
     "        for precision in range(1, 10):\n"
     "            text = \"%.*e\" % (precision - 1, value)\n"
     "            if _f32(float(text)) == value:\n"
     "                break\n"
     "    text = format(decimal.Decimal(text), \"f\")\n"
     "    if \".\" in text:\n"
     "        text = text.rstrip(\"0\").rstrip(\".\")\n"
     "    return text.encode()\n"},
    {"_index", "",
     "def _index(index):\n"
     "    if index < 0:\n"
     "        raise IndexError(\"index out of bounds\")\n"
     "    return index\n"},
    {"_text_slice", "",
     "def _text_slice(text, low, high=None):\n"
     "    if high is None:\n"
     "        high = len(text)\n"
     "    if low < 0 or low > high or high > len(text):\n"
     "        raise IndexError(\"slice range out of bounds\")\n"
     "    return text[low:high]\n"},
    {"_text_view", "",
     "def _text_view(view):\n"
     "    \"\"\"The bytes a []u8 view holds now, as a string.\"\"\"\n"
     "    return bytes(view.base[view.low:view.low + view.count])\n"},
    {"_view", "",
     "def _view(items, low, high):\n"
     "    \"\"\"A slice of a fixed array.\"\"\"\n"
     "    if low < 0 or low > high or high > len(items):\n"
     "        raise IndexError(\"slice range out of bounds\")\n"
     "    return ZiranSlice(items, low, high - low)\n"},
    {"_assign_items", "",
     "def _assign_items(target, source):\n"
     "    \"\"\"Overwrite an array of records or arrays in place.\"\"\"\n"
     "    for index, item in enumerate(source):\n"
     "        if type(item) is list:\n"
     "            _assign_items(target[index], item)\n"
     "        else:\n"
     "            target[index].assign(item)\n"},
    {"ZiranSlice", "",
     "class ZiranSlice:\n"
     "    \"\"\"A []T: count items of base starting at low.\"\"\"\n"
     "\n"
     "    __slots__ = (\"base\", \"low\", \"count\")\n"
     "\n"
     "    def __init__(self, base, low, count):\n"
     "        self.base = base\n"
     "        self.low = low\n"
     "        self.count = count\n"
     "\n"
     "    def __getitem__(self, index):\n"
     "        if index < 0 or index >= self.count:\n"
     "            raise IndexError(\"slice index out of bounds\")\n"
     "        return self.base[self.low + index]\n"
     "\n"
     "    def __setitem__(self, index, value):\n"
     "        if index < 0 or index >= self.count:\n"
     "            raise IndexError(\"slice index out of bounds\")\n"
     "        self.base[self.low + index] = value\n"
     "\n"
     "    def view(self, low, high=None):\n"
     "        if high is None:\n"
     "            high = self.count\n"
     "        if low < 0 or low > high or high > self.count:\n"
     "            raise IndexError(\"slice range out of bounds\")\n"
     "        return ZiranSlice(self.base, self.low + low, high - low)\n"
     "\n"
     "    @property\n"
     "    def data(self):\n"
     "        return ZiranPointer(self.base, self.low) if self.count else None\n"},
    {"ZiranPointer", "",
     "class ZiranPointer:\n"
     "    \"\"\"A *T: item key of a list, attribute key of an object, or the\n"
     "    object base itself when key is None.\"\"\"\n"
     "\n"
     "    __slots__ = (\"base\", \"key\")\n"
     "\n"
     "    def __init__(self, base, key=None):\n"
     "        self.base = base\n"
     "        self.key = key\n"
     "\n"
     "    @property\n"
     "    def value(self):\n"
     "        if self.key is None:\n"
     "            return self.base\n"
     "        if type(self.key) is str:\n"
     "            return getattr(self.base, self.key)\n"
     "        if self.key < 0:\n"
     "            raise IndexError(\"pointer out of bounds\")\n"
     "        return self.base[self.key]\n"
     "\n"
     "    @value.setter\n"
     "    def value(self, value):\n"
     "        if type(self.key) is str:\n"
     "            setattr(self.base, self.key, value)\n"
     "        elif self.key is None or self.key < 0:\n"
     "            raise IndexError(\"pointer out of bounds\")\n"
     "        else:\n"
     "            self.base[self.key] = value\n"
     "\n"
     "    def __getitem__(self, offset):\n"
     "        if offset == 0:\n"
     "            return self.value\n"
     "        return ZiranPointer(self.base, self.key + offset).value\n"
     "\n"
     "    def __setitem__(self, offset, value):\n"
     "        ZiranPointer(self.base, self.key + offset if offset else self.key).value = value\n"
     "\n"
     "    def __eq__(self, other):\n"
     "        return (isinstance(other, ZiranPointer) and other.base is self.base and\n"
     "                other.key == self.key)\n"
     "\n"
     "    def __hash__(self):\n"
     "        return hash((id(self.base), self.key))\n"},
    {"ZiranUnionArray", "struct",
     "class ZiranUnionArray:\n"
     "    \"\"\"An array field of a union: count items packed in its bytes.\"\"\"\n"
     "\n"
     "    __slots__ = (\"data\", \"format\", \"size\", \"count\")\n"
     "\n"
     "    def __init__(self, data, format, count):\n"
     "        self.data = data\n"
     "        self.format = \"=\" + format\n"
     "        self.size = struct.calcsize(self.format)\n"
     "        self.count = count\n"
     "\n"
     "    def __len__(self):\n"
     "        return self.count\n"
     "\n"
     "    def __getitem__(self, index):\n"
     "        if index < 0 or index >= self.count:\n"
     "            raise IndexError(\"index out of bounds\")\n"
     "        return struct.unpack_from(self.format, self.data, index * self.size)[0]\n"
     "\n"
     "    def __setitem__(self, index, value):\n"
     "        if isinstance(index, slice):\n"
     "            for position, item in enumerate(value):\n"
     "                self[position] = item\n"
     "            return\n"
     "        if index < 0 or index >= self.count:\n"
     "            raise IndexError(\"index out of bounds\")\n"
     "        struct.pack_into(self.format, self.data, index * self.size, value)\n"
     "\n"
     "    def __iter__(self):\n"
     "        return (self[index] for index in range(self.count))\n"
     "\n"
     "    def copy(self):\n"
     "        return list(self)\n"},
    {"ZiranVec", "",
     "class ZiranVec:\n"
     "    \"\"\"An owned Vec(T): the first count of capacity items.\"\"\"\n"
     "\n"
     "    __slots__ = (\"items\", \"count\", \"capacity\")\n"
     "\n"
     "    def __init__(self, items=None, count=0, capacity=0):\n"
     "        self.items = [] if items is None else items\n"
     "        self.count = count\n"
     "        self.capacity = capacity\n"
     "\n"
     "    def __getitem__(self, index):\n"
     "        if index < 0 or index >= self.count:\n"
     "            raise IndexError(\"Vec index out of bounds\")\n"
     "        return self.items[index]\n"
     "\n"
     "    def __setitem__(self, index, value):\n"
     "        if index < 0 or index >= self.count:\n"
     "            raise IndexError(\"Vec index out of bounds\")\n"
     "        self.items[index] = value\n"
     "\n"
     "    def copy(self):\n"
     "        return ZiranVec(self.items, self.count, self.capacity)\n"
     "\n"
     "    def assign(self, other):\n"
     "        self.items = other.items\n"
     "        self.count = other.count\n"
     "        self.capacity = other.capacity\n"
     "\n"
     "    @property\n"
     "    def data(self):\n"
     "        return ZiranPointer(self.items, 0) if self.capacity else None\n"},
    {"_vec_push", "",
     "def _vec_push(vector, value):\n"
     "    if vector.count == vector.capacity:\n"
     "        capacity = vector.capacity * 2 if vector.capacity else 4\n"
     "        if capacity > 0x7FFFFFFFFFFFFFFF:\n"
     "            return False\n"
     "        vector.items = vector.items[:vector.count] + [None] * (capacity - vector.count)\n"
     "        vector.capacity = capacity\n"
     "    vector.items[vector.count] = value\n"
     "    vector.count += 1\n"
     "    return True\n"},
    {"_vec_pop", "",
     "def _vec_pop(vector, option, zero):\n"
     "    if vector.count == 0:\n"
     "        return option(False, zero)\n"
     "    vector.count -= 1\n"
     "    return option(True, vector.items[vector.count])\n"},
    {"_vec_get", "",
     "def _vec_get(vector, index, option, zero, copy=None):\n"
     "    if index < 0 or index >= vector.count:\n"
     "        return option(False, zero)\n"
     "    item = vector.items[index]\n"
     "    return option(True, item if copy is None else copy(item))\n"},
    {"_vec_free", "",
     "def _vec_free(vector):\n"
     "    vector.items = []\n"
     "    vector.count = 0\n"
     "    vector.capacity = 0\n"},
    {"_vec_clone", "",
     "def _vec_clone(target, source, copy=None):\n"
     "    items = source.items[:source.count]\n"
     "    if copy is not None:\n"
     "        items = [copy(item) for item in items]\n"
     "    target.items = items\n"
     "    target.count = source.count\n"
     "    target.capacity = source.count\n"
     "    return True\n"},
    {"_vec_slice", "",
     "def _vec_slice(vector, low, high=None):\n"
     "    if high is None:\n"
     "        high = vector.count\n"
     "    if low < 0 or low > high or high > vector.count:\n"
     "        raise IndexError(\"Vec slice out of bounds\")\n"
     "    return ZiranSlice(vector.items, low, high - low)\n"},
    {"_vec_clear", "",
     "def _vec_clear(vector):\n"
     "    vector.count = 0\n"},
    {"_vec_swap", "",
     "def _vec_swap(first, second):\n"
     "    first.items, second.items = second.items, first.items\n"
     "    first.count, second.count = second.count, first.count\n"
     "    first.capacity, second.capacity = second.capacity, first.capacity\n"},
    {"_builder_append", "",
     "def _builder_append(builder, text):\n"
     "    for byte in text:\n"
     "        if not _vec_push(builder, byte):\n"
     "            return False\n"
     "    return True\n"},
    {"_builder_finish", "",
     "def _builder_finish(builder):\n"
     "    text = bytes(builder.items[:builder.count])\n"
     "    _vec_free(builder)\n"
     "    return text\n"},
    {"_library", "ctypes ctypes.util",
     "def _library(name):\n"
     "    \"\"\"Load a #system_library; the C library is the running process.\"\"\"\n"
     "    if name in (\"c\", \"libc\", \"m\", \"libm\"):\n"
     "        return ctypes.CDLL(None)\n"
     "    return ctypes.CDLL(ctypes.util.find_library(name) or \"lib%s.so\" % name)\n"},
    {"_link_library", "ctypes ctypes.util",
     "def _link_library(name):\n"
     "    \"\"\"Load a library named in LDLIBS so C foreign symbols resolve in it.\"\"\"\n"
     "    path = ctypes.util.find_library(name) or \"lib%s.so\" % name\n"
     "    ctypes.CDLL(path, mode=ctypes.RTLD_GLOBAL)\n"},
    {"_foreign", "ctypes",
     "def _foreign(library, symbol, argtypes, restype):\n"
     "    function = getattr(library, symbol)\n"
     "    if argtypes is not None:\n"
     "        function.argtypes = argtypes\n"
     "    function.restype = restype\n"
     "    return function\n"},
    {"_c_buffer", "ctypes",
     "def _c_buffer(pointer, ctype):\n"
     "    \"\"\"C storage holding the items a pointer reaches, and a way to copy\n"
     "    what C wrote back into them.\"\"\"\n"
     "    if pointer is None:\n"
     "        return None, None\n"
     "    if isinstance(pointer, int):\n"
     "        return ctypes.cast(ctypes.c_void_p(pointer), ctypes.POINTER(ctype)), None\n"
     "    items = pointer.base\n"
     "    start = pointer.key\n"
     "    if isinstance(items, ctypes._Pointer):\n"
     "        address = ctypes.cast(items, ctypes.c_void_p).value\n"
     "        address += start * ctypes.sizeof(items._type_)\n"
     "        return ctypes.cast(address, ctypes.POINTER(ctype)), None\n"
     "    buffer = (ctype * max(len(items) - start, 1))(*items[start:])\n"
     "\n"
     "    def write_back():\n"
     "        if isinstance(items, (bytes, str)):\n"
     "            return\n"
     "        for offset in range(len(items) - start):\n"
     "            items[start + offset] = buffer[offset]\n"
     "\n"
     "    return buffer, write_back\n"},
    {"_from_c_pointer", "ctypes ZiranPointer",
     "def _from_c_pointer(address, ctype):\n"
     "    if address is None or address == 0:\n"
     "        return None\n"
     "    if isinstance(address, ZiranPointer):\n"
     "        return address\n"
     "    return ZiranPointer(ctypes.cast(address, ctypes.POINTER(ctype)), 0)\n"},
    {"_c_call", "",
     "def _c_call(function, arguments, pointers):\n"
     "    \"\"\"Call C, passing each pointer argument as a C copy of its items\n"
     "    and copying back what the call changed.\"\"\"\n"
     "    arguments = list(arguments)\n"
     "    write_backs = []\n"
     "    for index, ctype in pointers:\n"
     "        arguments[index], write_back = _c_buffer(arguments[index], ctype)\n"
     "        if write_back is not None:\n"
     "            write_backs.append(write_back)\n"
     "    result = function(*arguments)\n"
     "    for write_back in write_backs:\n"
     "        write_back()\n"
     "    return result\n"},
    {"_c_text", "ctypes",
     "def _c_text(text):\n"
     "    return ctypes.c_char_p(bytes(text))\n"},
    {"ZiranCString", "ctypes",
     "class ZiranCString(ctypes.c_char_p):\n"
     "    \"\"\"A C string parameter or result of a foreign function.\"\"\"\n"},
    {"_c_string", "ctypes",
     "def _c_string(text):\n"
     "    \"\"\"A NUL-terminated copy of a Ziran string for C.\"\"\"\n"
     "    return ZiranCString(bytes(text))\n"},
    {"_from_c_string", "",
     "def _from_c_string(value):\n"
     "    \"\"\"The bytes of a C string result; NULL is the empty string.\"\"\"\n"
     "    return value.value or b\"\"\n"},
    {"Source_Code_Location", "",
     "class Source_Code_Location:\n"
     "    __slots__ = (\"fully_pathed_filename\", \"line_number\")\n"
     "\n"
     "    def __init__(self, fully_pathed_filename, line_number):\n"
     "        self.fully_pathed_filename = fully_pathed_filename\n"
     "        self.line_number = line_number\n"
     "\n"
     "    def copy(self):\n"
     "        return Source_Code_Location(self.fully_pathed_filename, self.line_number)\n"
     "\n"
     "    def assign(self, other):\n"
     "        self.fully_pathed_filename = other.fully_pathed_filename\n"
     "        self.line_number = other.line_number\n"},
    {"_py_module", "importlib",
     "def _py_module(name):\n"
     "    \"\"\"The Python module a py: foreign library names.\"\"\"\n"
     "    return importlib.import_module(name)\n"},
    {"_py_str", "",
     "def _py_str(text):\n"
     "    \"\"\"A Ziran string as Python str; invalid UTF-8 round-trips.\"\"\"\n"
     "    return bytes(text).decode(\"utf-8\", \"surrogateescape\")\n"},
    {"_py_bytes", "",
     "def _py_bytes(view):\n"
     "    \"\"\"A Ziran []u8 as Python bytes.\"\"\"\n"
     "    return bytes(view.base[view.low:view.low + view.count])\n"},
    {"_py_text", "",
     "def _py_text(value):\n"
     "    \"\"\"A Python result as a Ziran string: str is encoded as UTF-8,\n"
     "    bytes-like values are copied, None is empty, others use str().\"\"\"\n"
     "    if value is None:\n"
     "        return b\"\"\n"
     "    if isinstance(value, (bytes, bytearray, memoryview)):\n"
     "        return bytes(value)\n"
     "    if not isinstance(value, str):\n"
     "        value = str(value)\n"
     "    return value.encode(\"utf-8\", \"surrogateescape\")\n"},
    {"_py_u8", "",
     "def _py_u8(value):\n"
     "    \"\"\"A Python bytes-like or text result as a Ziran []u8.\"\"\"\n"
     "    data = list(_py_text(value))\n"
     "    return ZiranSlice(data, 0, len(data))\n"},
    {"_py_error", "",
     "def _py_error(error):\n"
     "    \"\"\"A raised exception as Ziran text: \\\"Type: message\\\".\"\"\"\n"
     "    message = str(error)\n"
     "    name = type(error).__name__\n"
     "    return _py_text(name + \": \" + message if message else name)\n"},
    {"_py_results", "",
     "def _py_results(call, record, value, convert, error, text):\n"
     "    \"\"\"Run a #py_results call: an exception fills the error field,\n"
     "    as text or as the exception object; a result fills value.\"\"\"\n"
     "    try:\n"
     "        returned = call()\n"
     "    except Exception as raised:\n"
     "        setattr(record, error, _py_error(raised) if text else raised)\n"
     "        return record\n"
     "    if value is not None:\n"
     "        setattr(record, value, returned if convert is None else convert(returned))\n"
     "    return record\n"},
    {"_py_callback", "",
     "def _py_callback(procedure, arguments, result):\n"
     "    \"\"\"A Ziran procedure Python calls with Python values.\"\"\"\n"
     "    def call(*values):\n"
     "        converted = [value if convert is None else convert(value)\n"
     "                     for convert, value in zip(arguments, values)]\n"
     "        returned = procedure(*converted)\n"
     "        return returned if result is None else result(returned)\n"
     "    return call\n"},
    {NULL, NULL, NULL}
};
