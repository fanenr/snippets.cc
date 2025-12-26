#include <stdexcept>

#include <boost/core/verbose_terminate_handler.hpp>
#include <boost/throw_exception.hpp>

void
foo (bool v)
{
  if (!v)
    BOOST_THROW_EXCEPTION (std::runtime_error ("v is false"));
}

int
main ()
{
  std::set_terminate (boost::core::verbose_terminate_handler);

  foo (false);
}
