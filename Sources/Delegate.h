#pragma once

#include <vector>
#include <memory>

template<typename TSignature> class Delegate;
template<typename TSignature> class ListenerBase;
template<typename TFunc, typename TSignature> class Listener;

template<typename TReturn, typename... TArgs>
class Delegate<TReturn(TArgs...)>
{
private:
	using TSignature = TReturn(TArgs...);
	std::vector<std::tuple<size_t, std::unique_ptr<ListenerBase<TSignature>>>> _listeners;

	size_t _id = 0;

public:
	template<typename TFunc>
	size_t AddListener(TFunc listener)
	{
		++_id;
		_listeners.emplace_back(_id, std::make_unique<Listener<TFunc, TSignature>>(listener));
		return _id;
	}

	void Clear()
	{
		_listeners.clear();
	}

	void RemoveListener(size_t id)
	{
		auto it = std::find_if(_listeners.begin(), _listeners.end(), [id](const auto &elem) { return std::get<0>(elem) == id; });
		_listeners.erase(it);
	}

	TReturn Invoke(TArgs... args)
	{
		if (_listeners.empty())
		{
			return TReturn{};
		}

		auto last = _listeners.end() - 1;
		for (auto it = _listeners.begin(); it != last; ++it)
		{
			std::get<1>(*it)->Call(args...);
		}

		// Return the return value of the last listener 
		return std::get<1>(*last)->Call(args...);
	}

	size_t GetListenerCount()
	{
		return _listeners.size();
	}
};

template<typename TReturn, typename... TArgs>
class ListenerBase<TReturn(TArgs...)>
{
public:
	virtual TReturn Call(TArgs... args) = 0;
};

template<typename TFunc, typename TReturn, typename... TArgs>
class Listener<TFunc, TReturn(TArgs...)> : public ListenerBase<TReturn(TArgs...)>
{
private:
	TFunc _f;

public:
	Listener(TFunc f) : _f(f) {}
	TReturn Call(TArgs... args) override
	{
		_f(args...);
	}
};

