##
# This file is part of Netsukuku
# (c) Copyright 2007 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
#
# This source code is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as published 
# by the Free Software Foundation; either version 2 of the License,
# or (at your option) any later version.
#
# This source code is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# Please refer to the GNU Public License for more details.
#
# You should have received a copy of the GNU Public License along with
# this source code; if not, write to:
# Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
##

from event import *

class Rem:
    """Route Efficiency Measure.
    
    This is a base class for different metrics (rtt, bandwidth, ...)"""

    def __init__(self, value, max_value=0, avgcoeff=1):
    	self.value=value

    	self.max_value=max_value	# Maximum value assumed by this REM
    	self.avgcoeff=avgcoeff		# Coefficient used for the average
    
    def __cmp__(self, b):
    	"""Compares two REMs
    	if remA > remB, then remA is better than remB
    	
    	NOTE: this means that if you have a list of rems and you
    	want to sort it in decrescent order of efficiency, than you
    	have to reverse sort it: list.sort(reverse=1)
    	"""
    	pass
    
    def __add__(self, b):
    	"""It sums two REMs.

    	The sum must be commutative, i.e. Rx+Ry=Ry+Rx"""
    	pass

class Rtt(Rem):
    """Round Trip Time"""

    def __init__(self, value, max_value=60*1000, avgcoeff=1):
    			  #1 minute in ms
    	Rem.__init__(self, value, max_value, avgcoeff)

    def __cmp__(self, b):
    	"""rtt comparison

    	self < b    1	 -->  The rtt `self' is better than `b'
    	self > b   -1	 -->  The rtt `self' is worse  than `b'
    	self = b    0	 -->  They are the same"""

    	return (self.value < b.value) - (self.value > b.value);
    
    def __add__(self, b):
    	return self.value+b.value

class Bw(Rem):
    """Bandwidth"""

    def __cmp__(self, b):
    	"""bandwidth comparison

    	self < b   -1	 -->  The bw `self' is worse  than `b'
    	self > b    1	 -->  The bw `self' is better than `b'
    	self = b    0	 -->  They are the same"""

    	return (self.value > b.value) - (self.value < b.value);
    
    def __add__(self, b):
    	return min(self.value, b.value)

class Avg(Rem):
    """Average"""
    
    def __init__(self, rems):
    	"""Calculates the average of different REMs.

    	`rems' is a list of type [R], where R is a Rem class, f.e. Rtt.
    	"""
    	
    	length=sum=0
    	for r in rems:
    		if not issubclass(r, Rem):
    			raise Exception, "an element of `rems' is not a Rem class"

    		sum+=abs( r.max_value-r.value*r.avgcoeff )
    		length+=1

    	Rem.__init__(self, sum/length)

    def __cmp__(self, b):
    	"""avg comparison

    	self < b   -1	 -->  The bw `self' is worse  than `b'
    	self > b    1	 -->  The bw `self' is better than `b'
    	self = b    0	 -->  They are the same"""

    	return (self.value > b.value) - (self.value < b.value);
    
    def __add__(self, b):
    	raise Exception, "the Avg metric cannot be summed. It must be computed each time"
    	pass


class RouteGw:
    """A route to a known destination.

    This class is intended for routes pointing to a same known destination.
    The only variables here are `gw', the gateway of the route, and `rem',
    its Rem"""

    def __init__(self, gw, rem):
	"""New gw route"""
    	self.gw	   = gw
    	self.rem   = rem
    
    def __cmp__(self, b):
	"""The route self is better (greater) than b iff its rem is better"""
        return self.rem.__cmp__(b.rem)

    def rem_modify(self, new_rem):
	"""Sets self.rem=new_rem and returns the old rem"""
    	if self.rem != new_rem:
    		oldrem=self.rem
    		self.rem=new_rem
		return oldrem
	return self.rem


class RouteNode:
    """List of routes to a known destination.

    This class is basically a list of RouteGw classes, where the
    destination node and its level are fixed and known.
    
    Note: for each gateway G there's only one route in self.routes, 
          which has the same gateway G
    """

    def __init__(self, 
		 lvl=None, id=None  # these are mandatory for Map.__init__(),
		 		    # but they aren't used
		):
    	self.routes = []

    	self.events = Event( [  'NEW_ROUTE',
    				'DEL_ROUTE',
    				'REM_ROUTE'	# the route's rem changed
    			     ] )

    def route_getby_gw(self, gw):
	"""Returns the route having as gateway `gw'"""
    	for r in self.routes:
    		if self.routes[r].gw == gw:
    			return self.routes[r]
    	return None

    def route_add(self, lvl, dst, gw, rem):
    	"""Add a route.

    	It returns 0 if the route hasn't been added, and thus it isn't
    	interesting, otherwise it returns 1 if it is a new route, 2 if
    	it substituted an old route."""

    	ret  = 0
    	oldr = self.route_getby_gw(gw)
    	
    	if self.is_empty() or
    		(oldr == None and rem > self.routes[-1]):
    	# If there aren't routes, or if it is better than the worst
    	# route, add it
    		self.routes.append(RouteGw(gw, rem))
    			self.events.send('NEW_ROUTE', (lvl, dst, gw, rem))
    		ret=1
    	elif oldr != None and rem > oldr.rem:
    		oldrem=oldr.rem_modify(rem)
    			self.events.send('REM_ROUTE', 
    					((lvl, dst, gw, rem), oldrem) )
    		ret=2
    	else:
    		return 0 # route not interesting

    	self.sort()

    	return ret	 # good route

    def route_del(self, lvl, dst, gw):
    	"""Delete a route.

    	Returns 1 if the route has been deleted, otherwise 0"""

    	r = route_getby_gw(gw)
    	if r != None:
		self.routes.remove(r)
		self.events.send('DEL_ROUTE', (lvl, dst, gw))
    		return 1
    	return 0
    
    def sort(self):
    	# Order the routes in decrescent order of efficiency, so that
    	# self.routes[0] is the best one
    	self.routes.sort(reverse=1)
    
    def is_empty(self):
    	return self.routes == []


class MapRoute(Map):
    """Map of routes, all of a same Rem type.

    MapRoute.node[lvl][id] is a RouteNode class, i.e. a list of routes
    having as destination the node (lvl, id)"""

    def __init__(self):
    	Map.__init__(self, levels, gsize, RouteNode)
    
    def route_add(self, lvl, dst, gw, rem):
    	return self.node_get(lvl, dst).route_add(lvl, dst, gw, rem)

    def route_del(self, lvl, dst, gw):
    	d=self.node_get(lvl, dst)
    	d.route_del(lvl, dst, gw)

    	if d.is_empty():
    		# No more routes to reach the node (lvl, dst).
    		# Consider it dead
    		self.node_del(lvl, dst)

    def routegw_del(self, gw):
    	"""Delete from the MapRoute all the routes passing from the
    	   gateway `gw'"""

    	for lvl in xrange(self.levels):
    		for dst in xrange(self.gsize):
    			self.route_del(lvl, dst, gw)
