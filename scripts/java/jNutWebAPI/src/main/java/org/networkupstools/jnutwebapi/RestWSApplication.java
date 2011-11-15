/* RestWSApplication.java

Copyright (C) 2011 Eaton

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
package org.networkupstools.jnutwebapi;

import java.util.HashSet;
import java.util.Set;
import javax.ws.rs.core.Application;

/**
 * Apache Wink tool to specify which are providers.
 * @author <a href="mailto:EmilienKia@eaton.com">Emilien Kia</a>
 */
public class RestWSApplication extends Application {

    @Override
    public Set<Class<?>> getClasses() {
        Set<Class<?>> classes = new HashSet<Class<?>>();
        classes.add(NutRestProvider.class);
        classes.add(ScannerProvider.class);
        return classes;
    }
}
