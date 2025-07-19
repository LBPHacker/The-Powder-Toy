#pragma once
#include <cstdint>
#include <map>
#include <span>
#include <stdexcept>
#include <variant>
#include <vector>
#include "common/String.h"
#include "common/VariantIndex.h"

class Bson
{
public:
	enum Type
	{
		objectValue,
		arrayValue,
		stringValue,
		userValue,
		doubleValue,
		int32Value,
		int64Value,
		boolValue,
	}; // keep this in sync with Wrapped's definition

	using String = ByteString;
	using Double = double;
	using Int32  = int32_t;
	using Int64  = int64_t;
	using Bool   = bool;
	using User   = std::vector<unsigned char>;

	using Object = std::map<String, Bson>;
	using Array  = std::vector<Bson>;

private:
	using Wrapped = std::variant<
		Object,
		Array,
		String,
		User,
		Double,
		Int32,
		Int64,
		Bool
	>;
	Wrapped wrapped;

public:
	Bson()                              = default;
	Bson(const Bson &other)             = default;
	Bson(Bson &&other)                  = default;
	Bson &operator =(const Bson &other) = default;
	Bson &operator =(Bson &&other)      = default;

	Bson(Type type) : wrapped(VariantAlternative<Wrapped>(std::size_t(type))) {}
	Bson(const Object  &alternative) : wrapped(                     alternative ) {}
	Bson(      Object &&alternative) : wrapped(std::forward<Object>(alternative)) {}
	Bson(const Array   &alternative) : wrapped(                     alternative ) {}
	Bson(      Array  &&alternative) : wrapped(std::forward<Array >(alternative)) {}
	Bson(const String  &alternative) : wrapped(                     alternative ) {}
	Bson(      String &&alternative) : wrapped(std::forward<String>(alternative)) {}
	Bson(const User    &alternative) : wrapped(                     alternative ) {}
	Bson(      User   &&alternative) : wrapped(std::forward<User  >(alternative)) {}
	Bson(Double         alternative) : wrapped(                     alternative ) {}
	Bson(Int32          alternative) : wrapped(                     alternative ) {}
	Bson(Int64          alternative) : wrapped(                     alternative ) {}
	Bson(Bool           alternative) : wrapped(                     alternative ) {}

	      Bson &operator [](std::size_t   key)       { return std::get<Array >(wrapped)     [key]        ; }
	const Bson &operator [](std::size_t   key) const { return std::get<Array >(wrapped)     [key]        ; }
	      Bson &operator [](const String &key)       { return std::get<Object>(wrapped)     [key]        ; }
	const Bson &operator [](const String &key) const { return std::get<Object>(wrapped).find(key)->second; }

	const Bson *Get(std::size_t   key) const { auto &arr = std::get<Array >(wrapped);                          if (key >= arr.size()) return nullptr; return &arr[key]  ; }
	const Bson *Get(const String &key) const { auto &obj = std::get<Object>(wrapped); auto it = obj.find(key); if (it  == obj.end() ) return nullptr; return &it->second; }

	      Object &AsObject()       { return std::get<Object>(wrapped); }
	const Object &AsObject() const { return std::get<Object>(wrapped); }
	      Array  &AsArray ()       { return std::get<Array >(wrapped); }
	const Array  &AsArray () const { return std::get<Array >(wrapped); }
	      String &AsString()       { return std::get<String>(wrapped); }
	const String &AsString() const { return std::get<String>(wrapped); }
	      User   &AsUser  ()       { return std::get<User  >(wrapped); }
	const User   &AsUser  () const { return std::get<User  >(wrapped); }
	Double        AsDouble() const { return std::get<Double>(wrapped); }
	Int32         AsInt32 () const { return std::get<Int32 >(wrapped); }
	Int64         AsInt64 () const { return std::get<Int64 >(wrapped); }
	Bool          AsBool  () const { return std::get<Bool  >(wrapped); }

	struct ParseError : std::runtime_error
	{
		using runtime_error::runtime_error;
	};
	static Bson Parse(std::span<const char> data, const Bson *rootNonconformance = nullptr);

	struct DumpError : std::runtime_error
	{
		using runtime_error::runtime_error;
	};
	std::vector<char> Dump(const Bson *rootNonconformance = nullptr) const;

	Type GetType() const { return Type(wrapped.index()); }
	bool IsObject() const { return GetType() == objectValue; };
	bool IsArray () const { return GetType() ==  arrayValue; };
	bool IsString() const { return GetType() == stringValue; };
	bool IsUser  () const { return GetType() ==   userValue; };
	bool IsDouble() const { return GetType() == doubleValue; };
	bool IsInt32 () const { return GetType() ==  int32Value; };
	bool IsInt64 () const { return GetType() ==  int64Value; };
	bool IsBool  () const { return GetType() ==   boolValue; };

	std::size_t GetSize() const
	{
		if (auto *object = std::get_if<Object>(&wrapped)) return object->size();
		if (auto *array  = std::get_if<Array >(&wrapped)) return array ->size();
		return 0;
	}

	Bson &Append(const Bson  &value) { return std::get<Array>(wrapped).emplace_back(                   value ); };
	Bson &Append(      Bson &&value) { return std::get<Array>(wrapped).emplace_back(std::forward<Bson>(value)); };
};
