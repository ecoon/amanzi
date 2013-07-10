import numpy
import matplotlib.pylab as plt
import math

class TransientTheis(object):
    """ Solves the system
         \frac{\partial^2{h}}{\partial^2{r}} + \frac{1}{r} \frac{\partial{h}}{\partial{r}} = \frac{S}{T} \frac{\partial{h}}{partial{r}}
         
         Initial Condition:
         h(r,0) = h_0 where h_0 is the initial heigth of the water table at t = 0.

         Boundary Conditions:
         h(\infin,t) = h_0 and constant pumping rate Q [volume/time].

         Theis Solution:
         s = \frac{Q}{4 \pi T} W(u)
         where,
         u = \frac{r^2 S}{4 T t} this quantity is a measure of aquifer response time. 
         W(u) = \int_u^\infty \frac{exp[-u]}{u} du = -0.5772 - ln(u) + u - \frac{u^2}{2*2!} + \frac{u^3}{3*3!} - ...

         Parameters are in units of:
         Q   : Pumping Rate [m^3/s]
         s   : Drawdown [m]
         h_0 : Initial height of water table [m]
         T   : Transmissivity [m^2/s]
         r   : radial distance measuresd outward from well [m]
         S   : Storage coefficient (unitless)
         t   : duration of pumping [s]
    """

    def __init__(self, params=None):
        if params is None:
            params = dict()

        params.setdefault("r", [20,40,50])
       # params.setdefault("x_low",-1)
       # params.setdefault("x_high",1)
       # params.setdefault("z_low",0)
       # params.setdefault("z_high",1)
       # params.setdefault("y_low",-1)
       # params.setdefault("y_high",1)
        params.setdefault("S_s",0.00001)
        params.setdefault("pi",math.pi)
        params.setdefault("g",9.80665)
        params.setdefault("K",1.e-12)
        params.setdefault("rho",1000)
        params.setdefault("mu",1.e-3)
        params.setdefault("h_0", 10)
        params.setdefault("Q",-1.e-3)
       
        self.__dict__.update(params)

        self.r
        
        self.K_h = self.K*self.g*self.rho / self.mu
        
        self.T =self. K_h*self.h_0
        
        self.var = -self.Q / 4 / self.pi /self.T
        
        self.S = self.S_s*self.h_0
        
    def runForFixedTime(self, radi, time):
        #This method evaluates Theis solution for a given time at multiple radial distances from the well 
        drawdown_r = []
        for r in radi:
            u = r ** 2 * self.S / 4 / self.T / time 
            W = getWellFunction(u)
            s = self.var*W
            drawdown_r.append(s)
        return drawdown_r

    def runForFixedRadius(self, times,radius):
        #This method evaluates Theis solution for a given radius at multiple progressions in time
        drawdown_t = []
        for t in times:
            u = radius ** 2 * self.S / 4 / self.T / t
            W = getWellFunction(u)
            s = self.var*W
            drawdown_t.append(s)
        return drawdown_t
 
def createFromXML(filename):
        
    #-- grab parameters from input XML file.
    import amanzi_xml.observations.ObservationXML as Obs_xml
    observations = Obs_xml.ObservationXML(filename)
    xml = observations.xml
    coords = observations.getAllCoordinates()
    import amanzi_xml.utils.search as search
    params = dict()
   
    params["r"] = []
    for coord in coords:
        params["r"].append(coord[0]) 
    
    params.setdefault("g",9.80665)
    params.setdefault("pi",math.pi)
    params["K"] = search.getElementByPath(xml, "/Main/Material Properties/Soil/Intrinsic Permeability: Uniform/Value").value
    params["mu"]= search.getElementByPath(xml, "/Main/Phase Definitions/Aqueous/Phase Properties/Viscosity: Uniform/Viscosity").value
    params["rho"]= search.getElementByPath(xml, "/Main/Phase Definitions/Aqueous/Phase Properties/Density: Uniform/Density").value
    params["h_0"]= search.getElementByPath(xml, "/Main/Boundary Conditions/Far Field Head/BC: Hydrostatic/Water Table Height").value[0]
    params["Q"]= search.getElementByPath(xml, "/Main/Sources/Pumping Well/Source: Volume Weighted/Values").value[0]
    params["S_s"]= search.getElementByPath(xml, "/Main/Material Properties/Soil/Specific Storage: Uniform/Value").value
    
    return TransientTheis(params)

def getWellFunction(U):
    u = float(U)
    a_0 =-0.57722
    a_1 = 0.99999
    a_2 =-0.24991
    a_3 = 0.05520
    a_4 =-0.00976
    a_5 = 0.00108
    b_1 = 8.57333
    b_2 = 18.05902
    b_3 = 8.63476
    b_4 = 0.26777
    c_1 = 9.57332
    c_2 = 25.63296
    c_3 = 21.09965
    c_4 = 3.95850

    try:
        u < 0
    except KeyError:
        raise RunTimeError("u cannot be negative check input times and radius!")
    if u < 1:
        W = -math.log(u) + a_0 + a_1*u + a_2*u**2 + a_3*u**3 + a_4*u**4 + a_5*u**5
        return W
    if u >= 1:
        W = (math.exp(-u) / u)*((u**4 +b_1*(u**3) + b_2*(u**2) + b_3*u + b_4)/(u**4 + c_1*(u**3) + c_2*(u**2) + c_3*u + c_4))
        return W

if __name__ == "__main__":
    #instaniate the class
    Tt = TransientTheis()
    
    radi = []
    rindex = numpy.arange(.1,50,.3)
    time = [1500,3600,86400,360000, 860000] #100 hours
    
    tindex=numpy.arange(100)
    times=[]

    for i in rindex:
        radi.append(i)

    for i in tindex:
        times.append(1+math.exp(float(i)*(i+1)/(8.5*len(tindex))))

    radius = [30,50,60]

    fig1 = plt.figure()
    fig2 = plt.figure()
    ax1 = fig1.add_axes([.1,.1,.8,.8]) 
    ax2 = fig2.add_axes([.1,.1,.8,.8])
    ax1.set_xlabel('Radial Distance from Well [m]')
    ax1.set_ylabel('Drawdown [m]')
    ax2.set_xlabel('Time after pumping [s]')
    ax2.set_ylabel('Drawddown [m]')
    ax1.set_title('Drawdown vs Radial Distance')
    ax2.set_title('Drawdown vs Time after Pumping')

    for r in radius:
        s_fixed_radius = Tt.runForFixedRadius(times, r)
        ax2.plot(times , s_fixed_radius, label ='$r=%0.1f m$'%r )
    
    for t in time: 
        s_fixed_time = Tt.runForFixedTime(radi, t)
        ax1.plot(radi , s_fixed_time, label = '$t=%0.1f s$'%t )
    
    ax1.legend(title='Theis Solution',loc = 'upper right', fancybox=True, shadow = True)
    ax2.legend(title = 'Theis Solution',loc = 'lower right', fancybox=True, shadow = True)

    plt.show()
    

    
    
