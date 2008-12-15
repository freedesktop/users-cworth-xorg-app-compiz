/*
 * Copyright © 2008 Dennis Kasprzyk
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
 */

#include <core/core.h>
#include <core/window.h>
#include <privatewindow.h>


CompWindow::Geometry::Geometry () :
    mBorder (0)
{
}

CompWindow::Geometry::Geometry (int x, int y,
				unsigned int width,
				unsigned int height,
				unsigned int border) :
    CompPoint (x, y),
    CompSize (width, height),
    mBorder (border)
{
}

unsigned int
CompWindow::Geometry::border () const
{
    return mBorder;
}

void
CompWindow::Geometry::setBorder (unsigned int border)
{
    mBorder = border;
}

void
CompWindow::Geometry::set (int x, int y,
			   unsigned int width,
			   unsigned int height,
			   unsigned int border)
{
    setX (x);
    setY (y);
    setWidth (width);
    setHeight (height);
    mBorder = border;
}

CompWindow::Geometry &
CompWindow::serverGeometry () const
{
    return priv->serverGeometry;
}

CompWindow::Geometry &
CompWindow::geometry () const
{
    return priv->geometry;
}


int
CompWindow::x () const
{
    return priv->geometry.x ();
}

int
CompWindow::y () const
{
    return priv->geometry.y ();
}

const CompPoint &
CompWindow::pos () const
{
    return priv->geometry;
}

/* With border */
unsigned int
CompWindow::width () const
{
    return priv->width;
}

unsigned int
CompWindow::height () const
{
    return priv->height;
}

const CompSize
CompWindow::size () const
{
    return CompSize (priv->width, priv->height);
}


int
CompWindow::serverX () const
{
    return priv->serverGeometry.x ();
}

int
CompWindow::serverY () const
{
    return priv->serverGeometry.y ();
}

const CompPoint &
CompWindow::serverPos () const
{
    return priv->serverGeometry;
}

/* With border */
unsigned int
CompWindow::serverWidth () const
{
    return priv->serverGeometry.width () + (2 * priv->serverGeometry.border ());
}

unsigned int
CompWindow::serverHeight () const
{
    return priv->serverGeometry.height () + (2 * priv->serverGeometry.border ());
}

const CompSize
CompWindow::serverSize () const
{
    return CompSize (
	priv->serverGeometry.width () + (2 * priv->serverGeometry.border ()),
	priv->serverGeometry.height () + (2 * priv->serverGeometry.border ()));
}