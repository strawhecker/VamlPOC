#pragma once
// VAML ItemsPanelTemplate — the items-host panel recipe (WPF
// System.Windows.Controls.ItemsPanelTemplate). 11n-3.
//
// The REAL `ItemsPanelTemplate` type lives in vaml/core/Scaffolding.h (a
// FrameworkTemplate-derived resource: set_panel_root + load_items_panel()).
// This header re-exports it so `#include <vaml/core/ItemsPanelTemplate.h>`
// resolves to the single, unified type — the 11n-3 DAG anchor
// (ItemsPanelTemplate, unified under FrameworkTemplate).
#include "vaml/core/Scaffolding.h"
