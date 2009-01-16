/*
 * Copyright © 2008 Dennis Kasprzyk
 * Copyright © 2007 Novell, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Dennis Kasprzyk not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Dennis Kasprzyk makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * DENNIS KASPRZYK DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL DENNIS KASPRZYK BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors: Dennis Kasprzyk <onestone@compiz-fusion.org>
 *          David Reveman <davidr@novell.com>
 */

#ifndef _COMPOPTION_H
#define _COMPOPTION_H

#include <compiz.h>

#include <vector>

class PrivateOption;
class PrivateValue;
class PrivateRestriction;
class CompAction;
class CompMatch;
class CompScreen;

class CompOption {
    public:
	typedef enum {
	    TypeBool,
	    TypeInt,
	    TypeFloat,
	    TypeString,
	    TypeColor,
	    TypeAction,
	    TypeKey,
	    TypeButton,
	    TypeEdge,
	    TypeBell,
	    TypeMatch,
	    TypeList
	} Type;

	class Value {
	    public:
		typedef std::vector<Value> Vector;

	    public:
		Value ();
		Value (const Value &);
		Value (const bool b);
		Value (const int i);
		Value (const float f);
		Value (const unsigned short *color);
		Value (const CompString& s);
		Value (const char *s);
		Value (const CompMatch& m);
		Value (const CompAction& a);
		Value (Type type, const Vector& l);
		~Value ();

		void set (const bool b);
		void set (const int i);
		void set (const float f);
		void set (const unsigned short *color);
		void set (const CompString& s);
		void set (const char *s);
		void set (const CompMatch& m);
		void set (const CompAction& a);
		void set (Type type, const Vector& l);

		bool               b ();
		int                i ();
		float              f ();
		unsigned short*    c ();
		CompString         s ();
		CompMatch &        match ();
		CompAction &       action ();
		Type               listType ();
		Vector &           list ();

		bool operator== (const Value& val);
		bool operator!= (const Value& val);
		Value & operator= (const Value &val);

	    private:
		PrivateValue *priv;
	};

	class Restriction {
	    public:
		Restriction ();
		Restriction (const Restriction &);
		~Restriction ();

		int iMin ();
		int iMax ();
		float fMin ();
		float fMax ();
		float fPrecision ();

		void set (int, int);
		void set (float, float, float);

		bool inRange (int);
		bool inRange (float);

		Restriction & operator= (const Restriction &rest);
	    private:
		PrivateRestriction *priv;
	};

	typedef std::vector<CompOption> Vector;

    public:
	CompOption ();
	CompOption (const CompOption &);
	CompOption (CompString name, Type type);
	~CompOption ();

	void setName (CompString name, Type type);
	
	CompString name ();

	Type type ();
	Value & value ();
	Restriction & rest ();

	bool set (Value &val);
	bool isAction ();

	CompOption & operator= (const CompOption &option);

    public:
	static CompOption * findOption (Vector &options, CompString name,
					unsigned int *index = NULL);

	static bool getBoolOptionNamed (Vector &options, CompString name,
					bool defaultValue = false);

	static int getIntOptionNamed (Vector &options, CompString name,
				      int defaultValue = 0);

	static float getFloatOptionNamed (Vector &options, CompString name,
					  float defaultValue = 0.0);

	static CompString getStringOptionNamed (Vector &options,
						CompString name,
						CompString defaultValue = "");

	static unsigned short * getColorOptionNamed (Vector &options,
						     CompString name,
						     unsigned short *defaultValue);

	static CompMatch & getMatchOptionNamed (Vector &options,
						CompString name,
						CompMatch &defaultValue);

	static CompString typeToString (Type type);

	static bool stringToColor (CompString     color,
				   unsigned short *rgba);

	static CompString colorToString (unsigned short *rgba);


	static bool setOption (CompOption  &o, Value &value);



    private:
	PrivateOption *priv;
};

extern CompOption::Vector noOptions;

#endif