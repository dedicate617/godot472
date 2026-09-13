/*
 * Variant.cc
 *
 *  Created on: May 15, 2019
 *      Author: alexej
 */

#include "OpcUaVariant.hh"
#include <sstream>

#ifdef __linux__
typedef unsigned char byte;
#endif

namespace OPCUA {

Variant::Variant()
{
	// custom deleter lambda for NativeVariantType
	auto uaVariantDeleter = [](NativeVariantType *uaVariant) {
        //UA_DEPRECATED
		//UA_Variant_deleteMembers(uaVariant);
		UA_Variant_clear(uaVariant);
		UA_Variant_delete(uaVariant);
	};
	// use unique pointer with custom deleter (see lambda) to manage the lifecycle of the internal NativeVariantType
	value = { UA_Variant_new(), uaVariantDeleter };
	UA_Variant_init(value.get());
}

/// native Variant copy constructor (makes a deep-copy of the value)
Variant::Variant(const NativeVariantType &uaValueRef)
:	Variant()
{
	UA_Variant_copy(&uaValueRef, this->value.get());
}

/// native Variant copy constructor (optionally takes ownership)
Variant::Variant(NativeVariantType *uaValuePtr, const bool &takeOwnership)
:	Variant()
{
	if(takeOwnership == true) {
		auto uaVariantDeleter = [](NativeVariantType *uaVariant) {
            //UA_DEPRECATED
            //UA_Variant_deleteMembers(uaVariant);
            UA_Variant_clear(uaVariant);
			UA_Variant_delete(uaVariant);
		};
		this->value.reset(uaValuePtr, uaVariantDeleter);
	} else {
		UA_Variant_copy(uaValuePtr, this->value.get());
	}
}

/// Variant copy constructor
Variant::Variant(const Variant &other)
:	Variant()
{
	UA_Variant_copy(other.value.get(), this->value.get());
}

/// Variant move constructor
Variant::Variant(Variant &&other)
{
	this->value = other.value;
}

void Variant::swap(Variant &other)
{
	this->value.swap(other.value);
}

/// Variant copy assignment operator
Variant& Variant::operator=(const Variant &other)
{
    //UA_DEPRECATED
	//UA_Variant_deleteMembers(value.get());
	UA_Variant_clear(value.get());
	UA_Variant_copy(other.value.get(), this->value.get());
	return *this;
}

/// Variant move assignment operator
Variant& Variant::operator=(Variant &&other)
{
	this->value = other.value;
	return *this;
}

bool Variant::isEmpty() const
{
	return UA_Variant_isEmpty(this->value.get());
}

bool Variant::isScalar() const
{
	if(isEmpty()) {
		return false;
	}
	return UA_Variant_isScalar(this->value.get());
}

int Variant::getTypeIndex() const
{
	const UA_DataType* typeIndex = value->type;
	
	if (typeIndex == &UA_TYPES[UA_TYPES_BOOLEAN]) {
		return UA_TYPES_BOOLEAN;
	}
	else if (typeIndex == &UA_TYPES[UA_TYPES_SBYTE]) {
		return UA_TYPES_SBYTE;
	}
	else if (typeIndex == &UA_TYPES[UA_TYPES_BYTE]) {
		return UA_TYPES_BYTE;
	}
	else if (typeIndex == &UA_TYPES[UA_TYPES_INT16]) {
		return UA_TYPES_INT16;
	}
	else if (typeIndex == &UA_TYPES[UA_TYPES_UINT16]) {
		return UA_TYPES_UINT16;
	}
	else if (typeIndex == &UA_TYPES[UA_TYPES_INT32]) {
		return UA_TYPES_INT32;
	}
	else if (typeIndex == &UA_TYPES[UA_TYPES_UINT32]) {
		return UA_TYPES_UINT32;
	}
	else if (typeIndex == &UA_TYPES[UA_TYPES_INT64]) {
		return UA_TYPES_INT64;
	}
	else if (typeIndex == &UA_TYPES[UA_TYPES_UINT64]) {
		return UA_TYPES_UINT64;
	}
	else if (typeIndex == &UA_TYPES[UA_TYPES_FLOAT]) {
		return UA_TYPES_FLOAT;
	}
	else if (typeIndex == &UA_TYPES[UA_TYPES_DOUBLE]) {
		return UA_TYPES_DOUBLE;
	}
	else if (typeIndex == &UA_TYPES[UA_TYPES_STRING]) {
		return UA_TYPES_STRING;
	}
	// TODO: datetime type converter
	else if (typeIndex == &UA_TYPES[UA_TYPES_DATETIME]) {
		return UA_TYPES_DATETIME;
	}
	else if (typeIndex == &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]) {
		return UA_TYPES_LOCALIZEDTEXT;
	}
	else if (typeIndex == &UA_TYPES[UA_TYPES_BYTESTRING]) {
		return UA_TYPES_BYTESTRING;
	}
	else if (typeIndex == &UA_TYPES[UA_TYPES_NODEID]) {
		return UA_TYPES_NODEID;
	}
	else if (typeIndex == &UA_TYPES[UA_TYPES_STATUSCODE]) {
		return UA_TYPES_STATUSCODE;
	}
	
	return -1;
}

std::string Variant::toString() const
{
	if(isEmpty()) {
		return std::string();
	}

	//auto typeIndex = value->type->typeIndex;
	auto typeIndex = value->type;
	if(isScalar()) {
		if( typeIndex == &UA_TYPES[UA_TYPES_BOOLEAN]) {
			auto boolValue = getValueAs<bool>();
			if(boolValue == true) {
				return std::string("true");
			} else {
				return std::string("false");
			}
		} 
		else if (typeIndex == &UA_TYPES[UA_TYPES_SBYTE]) {
			// all numeric types that fit into 32 bit
			auto intValue = getValueAs<char>();
			return std::to_string(intValue);
		}
		else if (typeIndex == &UA_TYPES[UA_TYPES_BYTE]) {
			// all numeric types that fit into 32 bit
			auto intValue = getValueAs<byte>();
			return std::to_string(intValue);
		}
		else if (typeIndex == &UA_TYPES[UA_TYPES_INT16]) {
			// all numeric types that fit into 32 bit
			auto intValue = getValueAs<short>();
			return std::to_string(intValue);
		}
		else if (typeIndex == &UA_TYPES[UA_TYPES_UINT16]) {
			// all numeric types that fit into 32 bit
			auto intValue = getValueAs<unsigned short>();
			return std::to_string(intValue);
		}
		else if( typeIndex == &UA_TYPES[UA_TYPES_INT32]) {
			// all numeric types that fit into 32 bit
			auto intValue = getValueAs<int>();
			return std::to_string(intValue);
		} else if( typeIndex == &UA_TYPES[UA_TYPES_UINT32]) {
			auto uintValue = getValueAs<unsigned int>();
			return std::to_string(uintValue);
		} else if( typeIndex == &UA_TYPES[UA_TYPES_INT64]) {
			auto intValue = getValueAs<long int>();
			return std::to_string(intValue);
		} else if( typeIndex == &UA_TYPES[UA_TYPES_UINT64]) {
			auto uintValue = getValueAs<unsigned long int>();
			return std::to_string(uintValue);
		} else if( typeIndex <= &UA_TYPES[UA_TYPES_DOUBLE]) {
			// floating types
			auto dblValue = getValueAs<double>();
			return std::to_string(dblValue);
		} else if (typeIndex == &UA_TYPES[UA_TYPES_STRING] || typeIndex == &UA_TYPES[UA_TYPES_BYTESTRING]) {
			UA_String *uaStringPtr = static_cast<UA_String*>(value->data);
			// reinterpret cast is quite a sledge hammer here
			//return UTF8ToString(std::string(reinterpret_cast<const char*>(uaStringPtr->data), uaStringPtr->length));
			//return wstringToUtf8(std::wstring()
			//std::string str = std::string(reinterpret_cast<const char*>(uaStringPtr->data), uaStringPtr->length);
			//return UTF8ToString(str);
			return std::string(reinterpret_cast<const char*>(uaStringPtr->data), uaStringPtr->length);
		} else if(typeIndex == &UA_TYPES[UA_TYPES_DATETIME]){
			UA_DateTime *uaDateTimePtr = static_cast<UA_DateTime*>(value->data);
			// case 1 return formatted
			//UA_DateTimeStruct dts = UA_DateTime_toStruct(*uaDateTimePtr);
			//return string_format("%u-%02u-%02u %02u:%02u:%02u.%03u", dts.year, dts.month, dts.day, dts.hour, dts.min, dts.sec, dts.milliSec);
			// case 2 return unix long int
			auto intValue = getValueAs<long int>();
			return std::to_string(intValue);
		}
		else if (typeIndex == &UA_TYPES[UA_TYPES_NODEID]) {
			UA_NodeId *uaNodeId = static_cast<UA_NodeId*>(value->data);
			std::stringstream ss;
			ss << uaNodeId->namespaceIndex << ":";
			if (uaNodeId->identifierType == UA_NODEIDTYPE_STRING) {
				ss << std::string((const char*)uaNodeId->identifier.string.data, uaNodeId->identifier.string.length);
				return ss.str();
			}
			else if (uaNodeId->identifierType == UA_NODEIDTYPE_NUMERIC) {
				ss << uaNodeId->identifier.numeric;
				return ss.str();
			}
			return std::string();
		}
		else if (typeIndex == &UA_TYPES[UA_TYPES_QUALIFIEDNAME]) {
			UA_QualifiedName *uaQname = static_cast<UA_QualifiedName*>(value->data);
			std::string index= std::to_string(uaQname->namespaceIndex);
			std::string simple_name(reinterpret_cast<const char*>(uaQname->name.data), uaQname->name.length);
			return index + ":" + simple_name;
		} else if(typeIndex == &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]) {
			UA_LocalizedText *uaText = static_cast<UA_LocalizedText*>(value->data);
			// reinterpret cast is quite a sledge hammer here
			return std::string(reinterpret_cast<const char*>(uaText->text.data), uaText->text.length);
		}
	} else {
		// is array type
		std::string result;
		if( typeIndex == &UA_TYPES[UA_TYPES_BOOLEAN]) {
			auto boolValues = getArrayValuesAs<bool>();
			for(size_t i=0; i<boolValues.size(); ++i) {
				if(boolValues[i] == true) {
					result = result + std::string("true");
				} else {
					result = result + std::string("false");
				}
				if(i < boolValues.size()-1) {
					result = result + ", ";
				}
			}
		} else if(typeIndex == &UA_TYPES[UA_TYPES_SBYTE] 
			|| typeIndex == &UA_TYPES[UA_TYPES_BYTE]
			|| typeIndex == &UA_TYPES[UA_TYPES_INT16] 
			|| typeIndex == &UA_TYPES[UA_TYPES_UINT16]
			|| typeIndex == &UA_TYPES[UA_TYPES_INT32]
			) {
			// all numeric types that fit into 32 bit
			auto intValues = getArrayValuesAs<int>();
			for(auto &ival: intValues) {
				result = result + std::to_string(ival);
				if(&ival != &intValues.back()) {
					result = result + ", ";
				}
			}
		} else if( typeIndex == &UA_TYPES[UA_TYPES_UINT32] ) {
			auto uintValues = getArrayValuesAs<unsigned int>();
			for(auto &uival: uintValues) {
				result = result + std::to_string(uival);
				if(&uival != &uintValues.back()) {
					result = result + ", ";
				}
			}
		} else if( typeIndex == &UA_TYPES[UA_TYPES_INT64] ) {
			auto intValues = getArrayValuesAs<long int>();
			for(auto &ival: intValues) {
				result = result + std::to_string(ival);
				if(&ival != &intValues.back()) {
					result = result + ", ";
				}
			}
		} else if( typeIndex == &UA_TYPES[UA_TYPES_UINT64] ) {
			auto uintValues = getArrayValuesAs<unsigned long int>();
			for(auto &uival: uintValues) {
				result = result + std::to_string(uival);
				if(&uival != &uintValues.back()) {
					result = result + ", ";
				}
			}
		} else if( typeIndex == &UA_TYPES[UA_TYPES_FLOAT]
			|| typeIndex == &UA_TYPES[UA_TYPES_DOUBLE]
			) {
			// floating types
			auto dblValues = getArrayValuesAs<double>();
			for(auto &dblval: dblValues) {
				result = result + std::to_string(dblval);
				if(&dblval != &dblValues.back()) {
					result = result + ", ";
				}
			}
		} else if(typeIndex == &UA_TYPES[UA_TYPES_STRING]) {
			auto strValues = getArrayValuesAs<std::string>();
			for(auto &str: strValues) {
				result = result + str;
				if(&str != &strValues.back()) {
					result = result + ", ";
				}
			}
		}
		return result;
	}

	return std::string();
}

std::string Variant::UTF8ToString(std::string& utf8Data) const
{
#ifdef _WIN32

	std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
	std::wstring wString = conv.from_bytes(utf8Data);    // utf-8 => wstring

	std::wstring_convert<std::codecvt< wchar_t, char, std::mbstate_t>>
		convert(new std::codecvt< wchar_t, char, std::mbstate_t>("CHS"));
	std::string str = convert.to_bytes(wString);     // wstring => string

	return str;
#elif defined(__linux__)
	return utf8Data;
#endif // __linux__
}

} /* namespace OPCUA */
