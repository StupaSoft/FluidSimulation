#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

template<typename TSignature> class Delegate;
template<typename TSignature> class ListenerBase;
template<typename TListener, typename TFunc, typename TSignature> class Listener;

template<typename TReturn, typename... TArgs>
class Delegate<TReturn(TArgs...)>
{
private:
	using TSignature = TReturn(TArgs...);
	std::vector<std::unique_ptr<ListenerBase<TSignature>>> _listeners;
	std::unordered_map<std::string, size_t> _uniqueListenerTable;

	size_t _registerID = 0;

public:
	// If callers want AddListener to remove the callback from the previous call to AddListener at the same location, they should pass functionNameAndLine
	template<typename TListener, typename TFunc>
	size_t AddListener(const std::weak_ptr<TListener> &listener, TFunc callback, const std::string &functionName = "", int lineNumber = -1)
	{
		if (auto listenerSP = listener.lock())
		{
			++_registerID;

			size_t listenerUID = listenerSP->GetListenerUID();
			if (listenerUID == -1)
			{
				throw std::runtime_error("The listener was not instantiated through Instantiate.");
			}

			if (functionName != "" && lineNumber != -1)
			{
				std::string functionNameAndLine = std::to_string(listenerUID) + functionName + std::to_string(lineNumber);
				if (_uniqueListenerTable.find(functionNameAndLine) != _uniqueListenerTable.end())
				{
					RemoveListener(_uniqueListenerTable.at(functionNameAndLine));
				}
				_uniqueListenerTable[functionNameAndLine] = _registerID;
			}
			_listeners.emplace_back(std::make_unique<Listener<TListener, TFunc, TSignature>>(_registerID, listener, callback));
		}
		else
		{
			throw std::runtime_error("Invalid listener.");
		}

		return _registerID;
	}

	void Clear()
	{
		_listeners.clear();
	}

	void RemoveListener(size_t id)
	{
		auto it = std::find_if(_listeners.begin(), _listeners.end(), [id](const auto &elem) { return elem->GetID() == id; });
		if (it != _listeners.end())
		{
			_listeners.erase(it);
		}
	}

	TReturn Invoke(TArgs... args)
	{
		if (_listeners.empty())
		{
			return TReturn{};
		}

		for (auto it = _listeners.begin(); it != _listeners.end() - 1; ++it)
		{
			const auto &listener = *it;
			if (listener->IsValid())
			{
				listener->Call(args...);
			}
			else
			{
				it = _listeners.erase(it);
			}
		}

		// Return the return value of the last listener
		return _listeners.empty() ? TReturn{} : (*(_listeners.end() - 1))->Call(args...);
	}

	size_t GetListenerCount() const
	{
		return _listeners.size();
	}
};

template<typename TReturn, typename... TArgs>
class ListenerBase<TReturn(TArgs...)>
{
public:
	virtual size_t GetID() const = 0;
	virtual bool IsValid() const = 0;
	virtual TReturn Call(TArgs... args) = 0;
};

template<typename TListener, typename TFunc, typename TReturn, typename... TArgs>
class Listener<TListener, TFunc, TReturn(TArgs...)> : public ListenerBase<TReturn(TArgs...)>
{
private:
	size_t _id;
	std::weak_ptr<TListener> _listener = nullptr;
	TFunc _callback;

public:
	Listener(size_t id, const std::weak_ptr<TListener> &listener, TFunc callback) : _id(id), _listener(listener), _callback(callback) {}

	size_t GetID() const override
	{
		return _id;
	}

	bool IsValid() const override
	{
		return _listener.lock() != nullptr;
	}

	TReturn Call(TArgs... args) override
	{
		_callback(args...);
	}
};

template<typename T>
class DelegateRegistrable : public std::enable_shared_from_this<DelegateRegistrable<T>>
{
private:
	size_t _uid = -1;
	static inline size_t _staticUID = -1;

public:
	template<typename TDerived, typename... TArgs>
	static std::shared_ptr<TDerived> Instantiate(TArgs&&... args)
	{
		++_staticUID;

		auto ptr = std::make_shared<TDerived>(std::forward<TArgs>(args)...);
		ptr->_uid = _staticUID;
		ptr->Register();

		return ptr;
	}

	virtual void Register() = 0;
	size_t GetListenerUID() { return _uid; }
};


