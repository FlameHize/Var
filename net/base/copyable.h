#ifndef VAR_BASE_COPYABLE_H
#define VAR_BASE_COPYABLE_H

namespace var
{

/// A tag class emphasises the objects are copyable.
/// The empty base class optimization applies.
/// Any derived class of copyable should be a value type.
class copyable
{
 protected:
  copyable() = default;
  ~copyable() = default;
};

}  // namespace var

#endif  // VAR_BASE_COPYABLE_H
