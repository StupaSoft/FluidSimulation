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
	std::vector<std::unique_ptr<ListenerBase<TSignature>>> _listeners;

public:
	template<typename TFunc>
	void AddListener(TFunc listener)
	{
		_listeners.emplace_back(std::make_unique<Listener<TFunc, TSignature>>(listener));
	}

	void Clear()
	{
		_listeners.clear();
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
			(*it)->Call(args...);
		}

		// Return the return value of the last listener 
		return (*last)->Call(args...);
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

