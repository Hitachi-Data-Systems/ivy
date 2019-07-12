//Copyright (c) 2016, 2017, 2018 Hitachi Vantara Corporation
//All Rights Reserved.
//
//   Licensed under the Apache License, Version 2.0 (the "License"); you may
//   not use this file except in compliance with the License. You may obtain
//   a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
//   WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
//   License for the specific language governing permissions and limitations
//   under the License.
//
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.
#pragma once

#include <math.h> // for sqrt
#include <queue>
#include <iostream>
#include <iomanip>
#include <exception>
#include <stdexcept>
#include <string>
#include <sstream>

template <typename FloatType> class LeastSquaresLine;

// author Allart Ian Vogelesang - April 26, 2015

// Implements the standard "least squares" method of fitting a line through a set of <x,y> points.

// Once you have put at least two <x,y> points in, it will do the calculations and
// you can call slope(), x_intercept(), y_intercept(), and correlation_coefficient().

template <typename FloatType> class LeastSquaresLine
{
private:
// private variables
	unsigned int lifetime {5};
		// If set to a positive number, when a "push()" is performed, if necessary the oldest points are deleted to keep to the max_size;
		// When lifetime == 0, there is no limit on the number of points kept.

	bool build_needed { true };
		// Methods retrieving calculation results first invoke build() if this is on.
		// Initially set by constructor to true.  Gets set to true when you push(), and by clear(); build() turns it off.
		// The build() function throws an exception if you try to read calculated output values before you post at least 2 x,y points.
		// All the calculated values are set the first time you invoke any output-retrieving
		// function such as slope(), x_intercept(), y_intercept(), and correlation_coefficient()

	FloatType x_bar, xx_bar, xy_bar, y_bar, yy_bar;  /* the aforementioned bar variables */

	FloatType sxx; // sum of ( (xi - x_bar) squared )   - number of points n times variance(x)
	FloatType syy; // sum of ( (yi - y_bar) squared )   - number of points n times variance(y)
	FloatType sxy; // sum of ( (xi - x_bar) * (yi - y_bar) ) - number of points n times covariance(x,y)

	FloatType alpha, beta; // our line is y = alpha + ( beta * x )

	std::deque<std::pair<FloatType /* x */, FloatType /* y */>> points;

// private methods
private:

	void build()
	{
		int n = points.size();

		if (n<2) throw std::runtime_error(std::string("<Error> LeastSquaresLine - must post at least two x-y points before trying to calculate results.  ") + toString());

		x_bar = xx_bar = xy_bar = y_bar = yy_bar = 0.0;

		for (std::pair<FloatType,FloatType>& point : points)
		{
			x_bar += point.first;
			y_bar += point.second;
		}

		x_bar /= (FloatType) n;
		y_bar /= (FloatType) n;

		sxx = syy = sxy = 0.0;

		for (std::pair<FloatType,FloatType>& point : points)
		{
			FloatType& x_i {point.first};
			FloatType& y_i {point.second};
			sxx += (x_i - x_bar) * (x_i - x_bar);
			syy += (y_i - y_bar) * (y_i - y_bar);
			sxy += (x_i - x_bar) * (y_i - y_bar);
		}

		if ( 0.0 == sxx ) throw std::runtime_error(std::string("<Error> LeastSquaresLine - variance of x values is zero.  Aarrgh!  ")+toString());

		beta = sxy / sxx;
		alpha = y_bar - (beta * x_bar);

		build_needed = false;

		return;
	}

// public variables - none

public:
// public methods
	int size() {return points.size();}
	LeastSquaresLine<FloatType>(){}
        LeastSquaresLine<FloatType>( int size ) : lifetime(size) {if (size<0) throw std::invalid_argument("<Error> LeastSquaresLine constructor fed negative number.");}

        void clear() {points.clear();}

        std::string push(FloatType x, FloatType y)
        {
       		std::ostringstream o;
       		o << "plotted_line.push(IOPS = " << std::fixed << std::setprecision(3) << x << ", WP_slew_per_second = " << std::fixed << std::setprecision(3) << (100.*y) << "%)";

        	points.push_front(std::make_pair(x,y));

        	if (lifetime) while (points.size() > lifetime)
        	{
        		o << " to keep to lifetime=" << lifetime << ", popping oldest point < " << std::fixed << std::setprecision(3) << points.back().first << ", WP_slew_per_second = " << std::fixed << std::setprecision(3) << (100.*points.back().second) << "% >";
        		points.pop_back();
        	}

        	build_needed = true;

///*debug*/	std::cout << "COPY points from here" << std::endl << std::endl;
///*debug*/	int debug_i=0;
///*debug*/	for (auto& point : points)
///*debug*/	{
///*debug*/		std::cout << debug_i++ << ',' << std::fixed << std::setprecision(1) << point.first << "," << std::fixed << std::setprecision(3) << (100.*point.second) << '%' << std::endl;
///*debug*/	}
///*debug*/
///*debug*/	std::cout << std::endl << "COPY points to here" << std::endl;

        	return o.str();
        }

        void set_lifetime( int size )
        {
        	if (size<0) throw std::invalid_argument( std::string("<Error> LeastSquaresLine set_lifeline() fed a negative number.  ") + toString() );
        	lifetime=size;

        	if (lifetime)
        	while (points.size() > lifetime) points.pop_back();

        	return;
       }

        FloatType correlation_coefficient()
        {
        	if (build_needed) build();
        	return sxy*sxy / (sxx * syy);
        }

	FloatType y_intercept() { if (build_needed) build(); return alpha; }
	FloatType x_intercept() { if (build_needed) build(); return - alpha / beta; }
	FloatType slope() { if (build_needed) build(); return beta; }

	std::string toString()
	{
		std::stringstream o;
		o << "LeastSquareLine: lifetime = " << lifetime << ", size = " << points.size();

		if (points.size() >= 2)
		{
			if (build_needed)
			{
				o << "build needed (a point has been pushed, but no results-retrieval methods have since been called)";
			}
			else
			{
				o 	<< " slope = " << std::scientific << std::setprecision(3) << slope()
					<< ", x_intercept() = " << std::fixed << std::setprecision(1) << x_intercept() << " steady WP drain IOPS"
					<< ", y_intercept = zero IOPS WP drain rate " << std::fixed << std::setprecision(3) << (100.*y_intercept()) << "% per second"
					<< ", correlation_coefficient() = " << std::fixed << std::setprecision(3) << correlation_coefficient();
			}
		}

		if (points.size())
		{
			ivy_float xmin, xmax, ymin, ymax;

			xmin = xmax = points.front().first;
			ymin = ymax = points.front().second;

			for (auto& point : points)
			{
				if (point.first < xmin) xmin = point.first;
				if (point.first > xmax) xmax = point.first;
				if (point.second < ymin) ymin = point.second;
				if (point.second > ymax) ymax = point.second;
			}

			o 	<< " IOPS min " << std::fixed << std::setprecision(1) << xmin << ", max " << std::fixed << std::setprecision(1) << xmax;

			ivy_float xrange = xmax - xmin;
			if (xrange>0.0)
			{
				o << " a " << std::fixed << std::setprecision(1) << (100.*xrange/(.5*(xmin+xmax))) << "% variation in IOPS";
			}

			ivy_float yrange = ymax - ymin;
			o 	<< ", WP change per second (slew rate) min="
					<< std::fixed << std::setprecision(3) << (100*ymin) << "%"
				<< ", max="
					<< std::fixed << std::setprecision(3) << (100*ymax) << "%";

			if (yrange>0.0 && (0.0 != (ymin+ymax)))
			{
				o << " a " << std::fixed << std::setprecision(1) << (100.*yrange/(.5*(ymin+ymax))) << "% variation in WP slew rate";
			}

			unsigned int max_count = 10, cur_count=0;

			o << ", ";
			if (points.size() > max_count) o << "first " << max_count << ' ';
			o << "points =";
			{
				for (auto& point : points)
				{
					if (++cur_count > max_count) break;
					o 	<< " < " << std::fixed << std::setprecision(1) << point.first
						<< " , " << std::fixed << std::setprecision(3) << (100.*point.second) << "% >";
				}
			}
		} // points.size()

		o << std::endl;

		return o.str();
	}

	void toOstream(std::ostream& os)
	{
		os << toString();
		// os << std::endl;
	}
};

