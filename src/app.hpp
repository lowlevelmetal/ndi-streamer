/**
 * @file app.hpp
 * @brief This file includes the main application class.
 * @date 2024-09-16
 * @author Matthew Todd Geiger
 * @version 1.0
 */

#pragma once

class App {
public:
	virtual AV::Utils::AvException Run() = 0;
};