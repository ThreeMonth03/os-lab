#include "kernel/line_editor.hpp"

namespace kernel {

bool LineEditor::insert(char value) { return buffer_.push_back(value); }

bool LineEditor::backspace() { return buffer_.pop_back(); }

void LineEditor::clear() { buffer_.clear(); }

} // namespace kernel
