from _base_class import base

class py_pl2pl(base):
	"""
	This problem represents a low-thrust transfer between a departure planet (default is the earth)
	and a target planet (default is mars). The spacecraft is described
	by its starting mass (mass) its engine specific impulse (Isp) and its engine maximum thrust (Tmax). The
	Sims-Flanagan model is used to describe a trajectory. A variable number of segments (nseg) can be used
	An initial velocity with respect to the Earth is allowed (Vinf) assumed to be given by the launcher
	The method high_fidelity allows to use a continuous thrust model rather than impulses
	"""

	def __init__(self,mass=1000,Tmax=0.05,Isp=2500,Vinf_0=3,Vinf_f=0,nseg=10,departure = None, target = None):
		"""__init__(self,mass=1000,Tmax=0.05,Isp=2500,Vinf=3,nseg=10,departure = earth, target = mars)"""
		try:
			import PyKEP
		except ImportError:
			print("Error while trying 'import PyKEP': is PyKEP installed?")
			raise
		if departure is None:
			departure = PyKEP.planet_ss('earth')
		if target is None:
			target = mars = PyKEP.planet_ss('mars')
		super(py_pl2pl,self).__init__(9 + nseg*3,0,1,9 + nseg,nseg+2,1e-5)
		self.__departure = departure
		self.__target = target
		self.__sc = PyKEP.sims_flanagan.spacecraft(mass,Tmax,Isp)
		self.__Vinf_0 = Vinf_0*1000
		self.__Vinf_f = Vinf_f*1000
		self.__leg = PyKEP.sims_flanagan.leg()
		self.__leg.set_mu(PyKEP.MU_SUN)
		self.__leg.set_spacecraft(self.__sc)
		self.__nseg = nseg
		self.set_bounds([0,60,self.__sc.mass/10,-self.__Vinf_0,-self.__Vinf_0,-self.__Vinf_0,-self.__Vinf_f,-self.__Vinf_f,-self.__Vinf_f] + [-1] * 3 *nseg,[3000,1500,self.__sc.mass,self.__Vinf_0,self.__Vinf_0,self.__Vinf_0,self.__Vinf_f,self.__Vinf_f,self.__Vinf_f] + [1] * 3 * nseg)
	def _objfun_impl(self,x):
		return (-x[2],)
	def _compute_constraints_impl(self,x):
		import PyKEP
		start = PyKEP.epoch(x[0])
		end = PyKEP.epoch(x[0] + x[1])
		r,v = self.__departure.eph(start)
		v_list = list(v)
		v_list[0] += x[3]
		v_list[1] += x[4]
		v_list[2] += x[5]
		x0 = PyKEP.sims_flanagan.sc_state(r,v_list,self.__sc.mass)
		r,v = self.__target.eph(end)
		xe = PyKEP.sims_flanagan.sc_state(r, v ,x[2])
		self.__leg.set(start,x0,x[-3 * self.__nseg:],end,xe)
		v_inf_con_0 = (x[3] * x[3] + x[4] * x[4] + x[5] * x[5] - self.__Vinf_0 * self.__Vinf_0) / (PyKEP.EARTH_VELOCITY * PyKEP.EARTH_VELOCITY)
		v_inf_con_f = (x[6] * x[6] + x[7] * x[7] + x[8] * x[8] - self.__Vinf_f * self.__Vinf_f) / (PyKEP.EARTH_VELOCITY * PyKEP.EARTH_VELOCITY)
		retval = list(self.__leg.mismatch_constraints() + self.__leg.throttles_constraints()) + [v_inf_con_0] + [v_inf_con_f]
		retval[0] /= PyKEP.AU
		retval[1] /= PyKEP.AU
		retval[2] /= PyKEP.AU
		retval[3] /= PyKEP.EARTH_VELOCITY
		retval[4] /= PyKEP.EARTH_VELOCITY
		retval[5] /= PyKEP.EARTH_VELOCITY
		retval[6] /= self.__sc.mass
		return retval
	def pretty(self,x):
		"""Decodes the decision vector x"""
		import PyKEP
		start = PyKEP.epoch(x[0])
		end = PyKEP.epoch(x[0] + x[1])
		r,v = self.__departure.eph(start)
		v_list = list(v)
		v_list[0] += x[3]
		v_list[1] += x[4]
		v_list[2] += x[5]
		x0 = PyKEP.sims_flanagan.sc_state(r,v_list,self.__sc.mass)
		r,v = self.__target.eph(end)
		xe = PyKEP.sims_flanagan.sc_state(r, v ,x[2])
		self.__leg.set(start,x0,x[-3 * self.__nseg:],end,xe)
		print("A direct interplantary transfer\n")
		print("FROM:")
		print(self.__departure)
		print("TO:")
		print(self.__target)
		print(self.__leg)
	def get_hf(self):
		return self.__leg.high_fidelity
	def set_hf(self,state):
		self.__leg.high_fidelity = state
	high_fidelity = property(get_hf,set_hf)
