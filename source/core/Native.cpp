/**
 * Copyright (C) 2015 crosire
 *
 * This software is  provided 'as-is', without any express  or implied  warranty. In no event will the
 * authors be held liable for any damages arising from the use of this software.
 * Permission  is granted  to anyone  to use  this software  for  any  purpose,  including  commercial
 * applications, and to alter it and redistribute it freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must not claim that you  wrote the
 *      original  software. If you use this  software  in a product, an  acknowledgment in the product
 *      documentation would be appreciated but is not required.
 *   2. Altered source versions must  be plainly  marked as such, and  must not be  misrepresented  as
 *      being the original software.
 *   3. This notice may not be removed or altered from any source distribution.
 */

#include "Native.hpp"
#include "ScriptDomain.hpp"
#include "Vector2.hpp"
#include "Vector3.hpp"

#include <NativeCaller.h>

using namespace System;
using namespace System::Collections::Generic;

namespace GTA
{
	namespace Native
	{
		namespace
		{
			private ref struct NativeTask : IScriptTask
			{
				virtual void Run()
				{
					nativeInit(_hash);

					for each (auto argument in _arguments)
					{
						nativePush64(argument->_data);
					}

					_result = nativeCall();
				}

				UInt64 _hash, *_result;
				array<InputArgument ^> ^_arguments;
			};

			UInt64 ObjectToNative(Object ^value)
			{
				if (Object::ReferenceEquals(value, nullptr))
				{
					return 0;
				}

				Type ^type = value->GetType();

				// Fundamental types
				if (type == Boolean::typeid)
				{
					return static_cast<bool>(value) ? 1 : 0;
				}
				if (type == Int32::typeid)
				{
					return static_cast<int>(value);
				}
				if (type == UInt32::typeid)
				{
					return static_cast<unsigned int>(value);
				}
				if (type == Single::typeid)
				{
					return BitConverter::ToUInt32(BitConverter::GetBytes(static_cast<float>(value)), 0);
				}
				if (type == Double::typeid)
				{
					return BitConverter::ToUInt32(BitConverter::GetBytes(static_cast<float>(static_cast<double>(value))), 0);
				}
				if (type == String::typeid)
				{
					return ScriptDomain::CurrentDomain->PinString(static_cast<String ^>(value)).ToInt64();
				}
				if (type == IntPtr::typeid)
				{
					return static_cast<IntPtr>(value).ToInt64();
				}

				throw gcnew InvalidCastException(String::Concat("Unable to cast object of type '", type->FullName, "' to native value"));
			}
			Object ^ObjectFromNative(Type ^type, UInt64 *value)
			{
				// Fundamental types
				if (type == Boolean::typeid)
				{
					return *reinterpret_cast<const int *>(value) != 0;
				}
				if (type == Int32::typeid)
				{
					return *reinterpret_cast<const int *>(value);
				}
				if (type == UInt32::typeid)
				{
					return *reinterpret_cast<const unsigned int *>(value);
				}
				if (type == Int64::typeid)
				{
					return *reinterpret_cast<const long long *>(value);
				}
				if (type == UInt64::typeid)
				{
					return *reinterpret_cast<const unsigned long long *>(value);
				}
				if (type == Single::typeid)
				{
					return *reinterpret_cast<const float *>(value);
				}
				if (type == Double::typeid)
				{
					return static_cast<double>(*reinterpret_cast<const float *>(value));
				}
				if (type == String::typeid)
				{
					if (*value != 0)
					{
						const auto size = static_cast<int>(strlen(reinterpret_cast<const char *>(*value)));
						const auto bytes = gcnew array<Byte>(size);

						Runtime::InteropServices::Marshal::Copy(static_cast<IntPtr>(static_cast<Int64>(*value)), bytes, 0, size);

						return Text::Encoding::UTF8->GetString(bytes);
					}
					else
					{
						return String::Empty;
					}
				}
				if (type == IntPtr::typeid)
				{
					return IntPtr(*reinterpret_cast<const long long *>(value));
				}

				// Math types
				if (type == Math::Vector2::typeid)
				{
					const auto data = reinterpret_cast<const float *>(value);

					return gcnew Math::Vector2(data[0], data[2]);
				}
				if (type == Math::Vector3::typeid)
				{
					const auto data = reinterpret_cast<const float *>(value);

					return gcnew Math::Vector3(data[0], data[2], data[4]);
				}

				throw gcnew InvalidCastException(String::Concat("Unable to cast native value to object of type '", type->FullName, "'"));
			}
		}

		InputArgument::InputArgument(Object ^value) : _data(ObjectToNative(value))
		{
		}
		OutputArgument::OutputArgument() : _storage(new unsigned char[24]()), InputArgument(IntPtr(_storage))
		{
		}
		OutputArgument::OutputArgument(Object ^value) : OutputArgument()
		{
			*reinterpret_cast<UInt64 *>(_storage) = ObjectToNative(value);
		}
		OutputArgument::~OutputArgument()
		{
			this->!OutputArgument();
		}
		OutputArgument::!OutputArgument()
		{
			delete[] _storage;
		}

		generic <typename T>
		T OutputArgument::GetResult()
		{
			return static_cast<T>(ObjectFromNative(T::typeid, reinterpret_cast<UINT64 *>(_data)));
		}

		generic <typename T>
		T Function::Call(Hash hash, ... array<InputArgument ^> ^arguments)
		{
			return Call<T>(static_cast<UInt64>(hash), arguments);
		}
		void Function::Call(Hash hash, ... array<InputArgument ^> ^arguments)
		{
			Call<int>(static_cast<UInt64>(hash), arguments);
		}
		generic <typename T>
		T Function::Call(UInt64 hash, ... array<InputArgument ^> ^arguments)
		{
			const auto task = gcnew NativeTask();
			task->_hash = hash;
			task->_arguments = arguments;

			ScriptDomain::CurrentDomain->ExecuteTask(task);

			return static_cast<T>(ObjectFromNative(T::typeid, task->_result));
		}
	}
}
