/**
  *
  * WMI
  * @author Thomas Sparber (2016)
  *
 **/

#ifndef WMI_HPP
#define WMI_HPP

#include <string>
#include <type_traits>

#include <tl/expected.hpp>
#include <wmiexception.hpp>
#include <wmiresult.hpp>

namespace Wmi
{

	//SFINAE test
	//default wmi path if wmi class does not implement getWmiPath
	template <class WmiClass>
	std::string CallGetWmiPath(...)
	{
		return "cimv2";
	}

	template <class WmiClass>
	typename std::enable_if<std::is_function<decltype(WmiClass::getWmiPath)>::value, std::string>::type
	CallGetWmiPath(int /* required, otherwise we get an ambiguitiy error... */)
	{
	   return WmiClass::getWmiPath();
	}

	tl::expected<void, WmiException> query(const std::string& q, const std::string& p, WmiResult &out);

	inline tl::expected<WmiResult, WmiException> query(const std::string& q, const std::string& p)
	{
		WmiResult result;
		auto status = query(q, p, result);
		if (!status) return tl::unexpected(status.error());
		return result;
	}

	template <class WmiClass>
	inline tl::expected<void, WmiException> retrieveWmi(WmiClass &out)
	{
		WmiResult result;
		const std::string q = std::string("Select * From ") + WmiClass::getWmiClassName();
		auto status = query(q, CallGetWmiPath<WmiClass>(0), result);
		if (!status) return tl::unexpected(status.error());
		out.setProperties(result, 0);
		return {};
	}

	template <class WmiClass>
	inline tl::expected<void, WmiException> retrieveWmi(WmiClass &out, std::string columns)
	{
		WmiResult result;
		const std::string q = std::string("Select ") + columns + std::string(" From ") + WmiClass::getWmiClassName();
		auto status = query(q, CallGetWmiPath<WmiClass>(0), result);
		if (!status) return tl::unexpected(status.error());
		out.setProperties(result, 0);
		return {};
	}

	template <class WmiClass>
	inline tl::expected<WmiClass, WmiException> retrieveWmi()
	{
		WmiClass temp;
		auto status = retrieveWmi(temp);
		if (!status) return tl::unexpected(status.error());
		return temp;
	}

	template <class WmiClass>
	inline tl::expected<WmiClass, WmiException> retrieveWmi(std::string columns)
	{
		WmiClass temp;
		auto status = retrieveWmi(temp, columns);
		if (!status) return tl::unexpected(status.error());
		return temp;
	}

	template <class WmiClass>
	inline tl::expected<void, WmiException> retrieveAllWmi(std::vector<WmiClass> &out)
	{
		WmiResult result;
		const std::string q = std::string("Select * From ") + WmiClass::getWmiClassName();
		auto status = query(q, CallGetWmiPath<WmiClass>(0), result);
		if (!status) return tl::unexpected(status.error());

		out.clear();
		for(std::size_t index = 0; index < result.size(); ++index)
		{
			WmiClass temp;
			temp.setProperties(result, index);
			out.push_back(std::move(temp));
		}
		return {};
	}

	template <class WmiClass>
	inline tl::expected<void, WmiException> retrieveAllWmi(std::vector<WmiClass> &out, std::string columns)
	{
		WmiResult result;
		const std::string q = std::string("Select ") + columns + std::string(" From ") + WmiClass::getWmiClassName();
		auto status = query(q, CallGetWmiPath<WmiClass>(0), result);
		if (!status) return tl::unexpected(status.error());

		out.clear();
		for(std::size_t index = 0; index < result.size(); ++index)
		{
			WmiClass temp;
			temp.setProperties(result, index);
			out.push_back(std::move(temp));
		}
		return {};
	}

	template <class WmiClass>
	inline tl::expected<std::vector<WmiClass>, WmiException> retrieveAllWmi()
	{
		std::vector<WmiClass> ret;
		auto status = retrieveAllWmi(ret);
		if (!status) return tl::unexpected(status.error());
		return ret;
	}

	template <class WmiClass>
	inline tl::expected<std::vector<WmiClass>, WmiException> retrieveAllWmi(std::string columns)
	{
		std::vector<WmiClass> ret;
		auto status = retrieveAllWmi(ret, columns);
		if (!status) return tl::unexpected(status.error());
		return ret;
	}

}; //end namespace Wmi

#endif //WMI_HPP