#ifndef VAR_BASE_NONCOPYABLE_H
#define VAR_BASE_NONCOPYABLE_H

namespace var
{

class noncopyable
{
 public:
  noncopyable(const noncopyable&) = delete;
  void operator=(const noncopyable&) = delete;

 protected:
  noncopyable() = default;
  ~noncopyable() = default;
};

}  // namespace var

#endif  // VAR_BASE_NONCOPYABLE_H
