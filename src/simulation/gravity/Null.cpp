#include "Gravity.h"
#include "Misc.h"
#include <cstring>

// gravity without fast Fourier transforms

void Gravity::get_result()
{
	*gravy = th_gravy;
	*gravx = th_gravx;
	*gravp = th_gravp;
}

void Gravity::update_grav(void)
{
}

GravityPtr Gravity::Create()
{
	return GravityPtr(new Gravity(CtorTag{}));
}

void GravityDeleter::operator ()(Gravity *ptr) const
{
	delete ptr;
}
