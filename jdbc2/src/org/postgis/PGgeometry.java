/*
 * PGgeometry.java
 * 
 * PostGIS extension for PostgreSQL JDBC driver - PGobject Geometry Wrapper
 * 
 * (C) 2004 Paul Ramsey, pramsey@refractions.net
 * 
 * (C) 2005 Markus Schaber, schabios@logi-track.com
 * 
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 2.1 of the License.
 * 
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA or visit the web at
 * http://www.gnu.org.
 * 
 * $Id$
 */

package org.postgis;

import org.postgis.binary.BinaryParser;
import org.postgresql.util.PGobject;

import java.sql.SQLException;

public class PGgeometry extends PGobject {

    Geometry geom;
    BinaryParser bp = new BinaryParser();

    public PGgeometry() {
        this.setType("geometry");
    }

    public PGgeometry(Geometry geom) {
        this();
        this.geom = geom;
    }

    public PGgeometry(String value) throws SQLException {
        this();
        setValue(value);
    }

    public void setValue(String value) throws SQLException {
        geom = geomFromString(value, bp);
    }

    public static Geometry geomFromString(String value) throws SQLException {
        BinaryParser bp = new BinaryParser();

        return geomFromString(value, bp);
    }

    /**
     * Maybe we could add more error checking here?
     */
    public static Geometry geomFromString(String value, BinaryParser bp) throws SQLException {
        value = value.trim();

        int srid = -1;

        if (value.startsWith("SRID")) {
            //break up geometry into srid and wkt
            String[] parts = PGgeometry.splitAtFirst(value, ';');
            value = parts[1];
            srid = Integer.parseInt(PGgeometry.splitAtFirst(parts[0], '=')[1]);
        }

        Geometry result;
        if (value.endsWith("EMPTY")) {
            // We have a standard conforming representation for an empty
            // geometry which is to be parsed as an empty GeometryCollection.
            result = new GeometryCollection();
        } else if (value.startsWith("MULTIPOLYGON")) {
            result = new MultiPolygon(value);
        } else if (value.startsWith("MULTILINESTRING")) {
            result = new MultiLineString(value);
        } else if (value.startsWith("MULTIPOINT")) {
            result = new MultiPoint(value);
        } else if (value.startsWith("POLYGON")) {
            result = new Polygon(value);
        } else if (value.startsWith("LINESTRING")) {
            result = new LineString(value);
        } else if (value.startsWith("POINT")) {
            result = new Point(value);
        } else if (value.startsWith("GEOMETRYCOLLECTION")) {
            result = new GeometryCollection(value);
        } else if (value.startsWith("00") || value.startsWith("01")) {
            result = bp.parse(value);
        } else {
            throw new SQLException("Unknown type: " + value);
        }

        if (srid != -1) {
            result.srid = srid;
        }

        return result;
    }

    public Geometry getGeometry() {
        return geom;
    }

    public int getGeoType() {
        return geom.type;
    }

    public String toString() {
        return geom.toString();
    }

    public String getValue() {
        return geom.toString();
    }

    public Object clone() {
        PGgeometry obj = new PGgeometry(geom);
        obj.setType(type);
        return obj;
    }

    /**
     * Splits a String at the first occurrence of border charachter.
     * 
     * Poor man's String.split() replacement, as String.split() was invented at
     * jdk1.4, and the Debian PostGIS Maintainer had problems building the woody
     * backport of his package using DFSG-free compilers. In all the cases we
     * used split() in the org.postgis package, we only needed to split at the
     * first occurence, and thus this code could even be faster.
     */
    public static String[] splitAtFirst(String whole, char border) {
        int index = whole.indexOf(border);
        if (index == -1) {
            return new String[]{whole};
        } else {
            return new String[]{
                whole.substring(0, index),
                whole.substring(index + 1)};
        }
    }
}