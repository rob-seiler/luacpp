#ifndef LUACPP_DETAIL_STATEVARIABLES_HPP
#define LUACPP_DETAIL_STATEVARIABLES_HPP

#include "../Basics.hpp"
#include "../Generic.hpp"
#include "../Stack.hpp"
#include "../StackGuard.hpp"
#include "../Table.hpp"
#include "../Type.hpp"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

struct lua_State;

namespace Lua {

/**
 * @brief Global-variable and global-table access facade.
 *
 * Reached as the @c variables member of a State: `state.variables.read<int>("x")`.
 * A thin view over the VM's @c lua_State — it forwards directly to the free
 * Basics / Stack / Table helpers and does NOT depend on State, so its template
 * bodies live inline here (no .inl, no include-order coupling). Holds the raw
 * VM pointer by value; neither copyable nor movable (its lifetime is tied to
 * the State that owns the VM).
 */
class Variables {
public:
	using TableFunction = std::function<void(Table&)>;

	explicit Variables(lua_State* state) : m_state(state) {}

	Variables(const Variables&) = delete;
	Variables& operator=(const Variables&) = delete;
	Variables(Variables&&) = delete;
	Variables& operator=(Variables&&) = delete;

	/**
	 * @brief Read a global variable, returning nullopt when missing or of the
	 *        wrong type.
	 *
	 * Does NOT invoke the configured error handler — "this global isn't there /
	 * isn't a T" is a query result, not a Lua-side error.
	 */
	template <typename T>
	[[nodiscard]] std::optional<T> read(const char* name) {
		Basics::pushGlobal(m_state, name);
		// tryGet never raises a Lua error (uses testUserData for class pointers),
		// so the plain popStack below always runs — no stack-cleanup hazard even
		// when the global has the wrong type. See Stack<T>::tryGet.
		std::optional<T> result = Stack<T>::tryGet(m_state, -1);
		Basics::popStack(m_state, 1);
		return result;
	}

	/// Write @p value into the global named @p name.
	template <typename T>
	void write(const char* name, T value) {
		Stack<T>::push(m_state, value);
		Basics::setGlobal(m_state, name);
	}

	/// Read a global table as a Generic→Generic map (empty if not a table).
	[[nodiscard]] std::map<Generic, Generic> readTableGeneric(const char* tableName);

	/// Read a global table as a Key→Value map (may throw TypeMismatchException).
	template <typename Key, typename Value>
	[[nodiscard]] std::map<Key, Value> readTable(const char* tableName) {
		std::map<Key, Value> result;
		const Type t = Basics::pushGlobal(m_state, tableName);
		DefaultStackGuard guard(m_state); // table.read can throw TypeMismatchException
		if (t == Type::Table) {
			Table table(m_state, -1);
			result = table.read<Key, Value>();
		}
		return result;
	}

	/// Read a global table, skipping entries whose key/value don't match Key/Value.
	template <typename Key, typename Value>
	[[nodiscard]] std::map<Key, Value> readTableIfMatching(const std::string& tableName) {
		std::map<Key, Value> result;
		const Type t = Basics::pushGlobal(m_state, tableName.c_str());
		DefaultStackGuard guard(m_state);
		if (t == Type::Table) {
			Table table(m_state, -1);
			result = table.readIfMatching<Key, Value>();
		}
		return result;
	}

	/// Write a string-keyed map into the global table named @p tableName.
	template <typename T>
	void writeTable(const char* tableName, const std::map<std::string, T>& map) {
		withTableDo(tableName, [&map](Table& table) {
			table.write(map);
		}, true);
	}

	/// Run @p workOnTable against the global table named @p tableName.
	void withTableDo(std::string_view tableName, TableFunction workOnTable, bool createIfMissing);

	/// Run @p workOnTable against the table at stack index @p index.
	void withTableDo(int index, TableFunction workOnTable);

private:
	lua_State* m_state;
};

} // namespace Lua

#endif // LUACPP_DETAIL_STATEVARIABLES_HPP
