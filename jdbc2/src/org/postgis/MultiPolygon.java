/*
 * MultiPolygon.java
 * 
 * PostGIS extension for PostgreSQL JDBC driver - geometry model
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

import org.postgresql.util.PGtokenizer;

import java.sql.SQLException;

public class MultiPolygon extends ComposedGeom {

    public MultiPolygon() {
        super(MULTIPOLYGON);
    }

    public MultiPolygon(Polygon[] polygons) {
        super(MULTIPOLYGON, polygons);
    }

    public MultiPolygon(String value) throws SQLException {
        this();
        value = value.trim();
        if (value.indexOf("MULTIPOLYGON") == 0) {
            PGtokenizer t = new PGtokenizer(PGtokenizer.removePara(value.substring(12).trim()), ',');
            int npolygons = t.getSize();
            subgeoms = new Polygon[npolygons];
            for (int p = 0; p < npolygons; p++) {
                subgeoms[p] = new Polygon(t.getToken(p));
            }
            dimension = subgeoms[0].dimension;
        } else {
            throw new SQLException("postgis.multipolygongeometry");
        }
    }

    public int numPolygons() {
        return subgeoms.length;
    }

    public Polygon getPolygon(int idx) {
        if (idx >= 0 & idx < subgeoms.length) {
            return (Polygon) subgeoms[idx];
        } else {
            return null;
        }
    }

}