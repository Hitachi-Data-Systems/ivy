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

// started with code from http://www.johndcook.com/standard_deviation.html
// and then http://www.johndcook.com/skewness_kurtosis.html
// and then many updates by Ian Vogelesang, including templates

// 2016-03-24 - when asked if I could use it in ivy as open source John D. Cook gave permission by email to use this derived version of his original code in any way.

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath> // for sqrt()
#include <string>

using namespace std;

template <typename FloatType, typename IntType> class RunningStat;

template <typename FloatType, typename IntType> RunningStat<FloatType, IntType> operator+(const RunningStat<FloatType,IntType>& a, const RunningStat<FloatType,IntType>& b);

template <typename FloatType, typename IntType>
class RunningStat {

private:
// variables
        IntType n {0};
        FloatType M1 {0.}, M2 {0.};
#ifdef KURTOSISKEW
        FloatType M3 {0.}, M4 {0.};
#endif // KURTOSISKEW
	FloatType min_value {0.}, max_value {0.};

// methods

// This only has a few built-in integer & float member variables, so default constructors/destructors/copies work fine.

public:
        RunningStat<FloatType,IntType>();
	friend RunningStat<FloatType,IntType> operator+<FloatType, IntType>(const RunningStat<FloatType,IntType>& a, const RunningStat<FloatType,IntType>& b);
 	RunningStat<FloatType,IntType>& operator+=(const RunningStat<FloatType,IntType> &rhs);

        void clear();
        void no_op();
        void push(FloatType x);
        unsigned int count() const;
	FloatType min() const;
	FloatType max() const;
	FloatType sum() const;
        FloatType avg() const;
        FloatType variance() const;
        FloatType standardDeviation() const;
#ifdef KURTOSISKEW
	FloatType skewness() const;
	FloatType kurtosis() const;
#endif // KURTOSISKEW
	std::string toString() const;
	bool fromIstream(std::istream &);
	bool fromString(const std::string&);

	void printme(std::ostream& o) const;
};


template <typename FloatType, typename IntType>
RunningStat<FloatType, IntType>::RunningStat()
{
	clear();
}

template <typename FloatType, typename IntType>
void RunningStat<FloatType, IntType>::clear()
{
	n=0;
	min_value=max_value=M1=M2=0.;
#ifdef KURTOSISKEW
	M3=M4=0.;
#endif // KURTOSISKEW
}

template <typename FloatType, typename IntType>
void RunningStat<FloatType, IntType>::no_op()
{
    return;
}

template <typename FloatType, typename IntType>
void RunningStat<FloatType, IntType>::push(FloatType x)
{
	FloatType delta, delta_n, term1;
#ifdef KURTOSISKEW
	FloatType delta_n2;
#endif // KURTOSISKEW

	long long n1 = n;
	n++;
	delta = x - M1;
	delta_n = delta / n;
#ifdef KURTOSISKEW
	delta_n2 = delta_n * delta_n;
#endif // KURTOSISKEW
	term1 = delta * delta_n * n1;
	M1 += delta_n;
#ifdef KURTOSISKEW
	M4 += term1 * delta_n2 * (n*n - 3*n + 3) + 6 * delta_n2 * M2 - 4 * delta_n * M3;
	M3 += term1 * delta_n * (n - 2) - 3 * delta_n * M2;
#endif // KURTOSISKEW
	M2 += term1;
	if (n==1) {
		min_value=max_value=x;
	} else {
		if (x<min_value) min_value=x;
		if (x>max_value) max_value=x;
	}
}

template <typename FloatType, typename IntType>
unsigned int RunningStat<FloatType, IntType>::count() const
{
	 return n;
}

template <typename FloatType, typename IntType>
FloatType RunningStat<FloatType, IntType>::avg() const
{
	 return M1;
}

template <typename FloatType, typename IntType>
FloatType RunningStat<FloatType, IntType>::variance() const
{
	if (n>1)
		return M2/(n-1.0);
	else
		return 0.;
}

template <typename FloatType, typename IntType>
FloatType RunningStat<FloatType, IntType>::standardDeviation() const
{
	 return sqrt( variance() );
}

#ifdef KURTOSISKEW
template <typename FloatType, typename IntType>
FloatType RunningStat<FloatType, IntType>::skewness() const
{
	if (n>1)
		return sqrt(double(n)) * M3/ pow(M2, 1.5);
	else
		return 0;
}

template <typename FloatType, typename IntType>
FloatType RunningStat<FloatType, IntType>::kurtosis() const
{
	if (n>1)
		return double(n)*M4 / (M2*M2) - 3.0;
	else
		return 0;
}
#endif // KURTOSISKEW

template <typename FloatType, typename IntType>
RunningStat<FloatType,IntType> operator+(const RunningStat<FloatType, IntType>& a, const RunningStat<FloatType,IntType>& b)
{
	if (0==b.n) return a;
	if (0==a.n) return b;

	RunningStat<FloatType,IntType> combined;

	combined.n += a.n + b.n;

	FloatType delta = b.M1 - a.M1;
	FloatType delta2 = delta*delta;
#ifdef KURTOSISKEW
	FloatType delta3 = delta*delta2;
	FloatType delta4 = delta2*delta2;
#endif // KURTOSISKEW

	combined.M1 = (a.n*a.M1 + b.n*b.M1) / combined.n;

	combined.M2 = a.M2 + b.M2 +
	               delta2 * a.n * b.n / combined.n;

#ifdef KURTOSISKEW
	combined.M3 = a.M3 + b.M3 +
	               delta3 * a.n * b.n * (a.n - b.n)/(combined.n*combined.n);

	combined.M3 += 3.0*delta * (a.n*b.M2 - b.n*a.M2) / combined.n;

	combined.M4 = a.M4 + b.M4 + delta4*a.n*b.n * (a.n*a.n - a.n*b.n + b.n*b.n) /
	               (combined.n*combined.n*combined.n);

	combined.M4 += 6.0*delta2 * (a.n*a.n*b.M2 + b.n*b.n*a.M2)/(combined.n*combined.n) +
	               4.0*delta*(a.n*b.M3 - b.n*a.M3) / combined.n;
#endif // KURTOSISKEW

	if (a.min_value<b.min_value) combined.min_value=a.min_value; else combined.min_value=b.min_value;
	if (a.max_value>b.max_value) combined.max_value=a.max_value; else combined.max_value=b.max_value;

	return combined;
}

template <typename FloatType, typename IntType>
RunningStat<FloatType,IntType>& RunningStat<FloatType, IntType>::operator+=(const RunningStat<FloatType,IntType>& rhs)
{
	     RunningStat<FloatType,IntType> combined = *this + rhs;
	     *this = combined;
	     return *this;
}


template <typename FloatType, typename IntType>
FloatType RunningStat<FloatType,IntType>::min() const {
	return min_value;
}

template <typename FloatType, typename IntType>
FloatType RunningStat<FloatType, IntType>::max() const {
	return max_value;
}

template <typename FloatType, typename IntType>
FloatType RunningStat<FloatType, IntType>::sum() const {
	return n*M1;
}

template <typename FloatType, typename IntType>
void RunningStat<FloatType, IntType>::printme(std::ostream& o) const {
	o << "count = " << n << ", total=" << sum() << ", avg=" << avg() << ", min=" << min_value << ", max=" << max_value << ", std_dev=" << standardDeviation();
#ifdef KURTOSISKEW
	o << ", skewness = " << Skewness() << ", kurtosis = " << Kurtosis();
#endif // KURTOSISKEW
}

template <typename FloatType, typename IntType>
std::string RunningStat<FloatType, IntType>::toString() const
{
	ostringstream o;
	o << '<' << n << ';' << M1 << ';' << M2;
#ifdef KURTOSISKEW
	o << ';' << M3 << ';' << M4;
#endif // KURTOSISKEW
	o << ';' << min_value << ';' << max_value << '>';
	return o.str();
}

template <typename FloatType, typename IntType>
bool RunningStat<FloatType, IntType>::fromIstream(std::istream& i)
{
	char c;
	i >> c; if (i.fail() || c != '<') 	{clear();return false;}
	i >> n; if (i.fail()) 			{clear();return false;}
	i >> c; if (i.fail() || c != ';') 	{clear();return false;}
	i >> M1; if (i.fail()) 			{clear();return false;}
	i >> c; if (i.fail() || c != ';') 	{clear();return false;}
	i >> M2; if (i.fail()) 			{clear();return false;}
#ifdef KURTOSISKEW
	i >> c; if (i.fail() || c != ';') 	{clear();return false;}
	i >> M3; if (i.fail()) 			{clear();return false;}
	i >> c; if (i.fail() || c != ';') 	{clear();return false;}
	i >> M4; if (i.fail()) 			{clear();return false;}
#endif // KURTOSISKEW
	i >> c; if (i.fail() || c != ';') 	{clear();return false;}
	i >> min_value; if (i.fail()) 		{clear();return false;}
	i >> c; if (i.fail() || c != ';') 	{clear();return false;}
	i >> max_value; if (i.fail()) 		{clear();return false;}
	i >> c; if (i.fail() || c != '>') 	{clear();return false;}
	return true;
}

template <typename FloatType, typename IntType>
bool RunningStat<FloatType, IntType>::fromString(const std::string& s) {
	std::istringstream is(s);
	if (!fromIstream(is)) return false;
	char c;
	is >> c;
	if (!is.fail()) return false;
	return true;
}
