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

	std::vector<std::unique_ptr<ListenerBase<TSignature>>> _reservedAdditions; // Elements that will be newly added to this delegate.
	size_t _invalidatedListenerCount = 0;

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

			_reservedAdditions.emplace_back(std::make_unique<Listener<TListener, TFunc, TSignature>>(_registerID, listener, callback));
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
		_reservedAdditions.clear();
		_invalidatedListenerCount = 0;
	}

	void RemoveListener(size_t id)
	{
		auto itListener = std::find_if(_listeners.begin(), _listeners.end(), [id](const auto &listener) { return listener->GetID() == id; });
		if (itListener != _listeners.end())
		{
			(*itListener)->Invalidate();
			++_invalidatedListenerCount;
		}

		auto itReserved = std::find_if(_reservedAdditions.begin(), _reservedAdditions.end(), [id](const auto &listener) { return listener->GetID() == id; });
		if (itReserved != _reservedAdditions.end())
		{
			_reservedAdditions.erase(itReserved);
		}
	}

	TReturn Invoke(TArgs... args)
	{
		// The listeners can add or remove listeners to this Delegate in a recursive manner.
		// In such cases, we should commit addition or removal of callbacks only after invocation of all listeners in a delegate
		// to reduce confusive behaviors and prevent the use of invalidated iterators.

		// Remove listeners that were invalidated
		if (_invalidatedListenerCount > 0)
		{
			_listeners.erase(std::remove_if(_listeners.begin(), _listeners.end(), [](const auto &listener) { return listener->IsInvalidated(); }), _listeners.end());
			_invalidatedListenerCount = 0;
		}

		// Commit reserved additions
		if (_reservedAdditions.size() > 0)
		{
			_listeners.insert(_listeners.end(), std::make_move_iterator(_reservedAdditions.begin()), std::make_move_iterator(_reservedAdditions.end()));
			_reservedAdditions.clear();
		}

		// Invoke callbacks
		if (_listeners.empty()) return TReturn{};
		for (auto it = _listeners.begin(); it != _listeners.end() - 1; ++it)
		{
			const auto &listener = *it;
			if (listener->IsAlive())
			{
				listener->Call(args...);
			}
			else
			{
				listener->Invalidate();
				++_invalidatedListenerCount;
			}
		}

		// Return the return value of the last listener
		const auto &listener = *(_listeners.end() - 1);
		if (listener->IsAlive())
		{
			return listener->Call(args...);
		}
		else
		{
			listener->Invalidate();
			++_invalidatedListenerCount;

			return TReturn{};
		}
	}

	size_t GetListenerCount() const
	{
		return _listeners.size() + _reservedAdditions.size();
	}
};

template<typename TReturn, typename... TArgs>
class ListenerBase<TReturn(TArgs...)>
{
public:
	virtual size_t GetID() const = 0;
	virtual bool IsAlive() const = 0;
	virtual bool IsInvalidated() const = 0;
	virtual void Invalidate() = 0;
	virtual TReturn Call(TArgs... args) = 0;
};

template<typename TListener, typename TFunc, typename TReturn, typename... TArgs>
class Listener<TListener, TFunc, TReturn(TArgs...)> : public ListenerBase<TReturn(TArgs...)>
{
private:
	size_t _id;
	std::weak_ptr<TListener> _listener = nullptr;
	TFunc _callback;
	bool _invalidated = false;

public:
	Listener(size_t id, const std::weak_ptr<TListener> &listener, TFunc callback) : _id(id), _listener(listener), _callback(callback) {}

	size_t GetID() const override
	{
		return _id;
	}

	bool IsAlive() const override
	{
		return _listener.lock() != nullptr;
	}

	bool IsInvalidated() const override
	{
		return _invalidated;
	}

	void Invalidate() override
	{
		_invalidated = true;
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

	virtual void Register() {};
	size_t GetListenerUID() { return _uid; }
};


