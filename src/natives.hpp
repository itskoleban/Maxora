/**
 * @file natives.hpp
 * @brief Native AMX functions registration interface.
 */

#pragma once

#include <Server/Components/Pawn/pawn.hpp>

namespace maxora
{
	/**
	 * @brief Registers all the Maxora AMX natives into the given script instance.
	 * @param script The Pawn script instance receiving the natives.
	 */
	void RegisterNatives(IPawnScript& script);
} // namespace maxora
