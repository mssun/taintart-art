/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

public class Linking {
  public static boolean canAccess(String className, boolean takesParameter) throws Exception {
    try {
      Class<?> c = Class.forName(className);
      if (takesParameter) {
        c.getDeclaredMethod("access", Integer.TYPE).invoke(null, 42);
      } else {
        c.getDeclaredMethod("access").invoke(null);
      }
      return true;
    } catch (InvocationTargetException ex) {
      if (ex.getCause() instanceof NoSuchFieldError || ex.getCause() instanceof NoSuchMethodError) {
        return false;
      } else {
        throw ex;
      }
    }
  }

  public static boolean canOverride(String methodName) throws Exception {
    // ParentClass returns only positive numbers, OverrideClass only negative.
    // This way we can tell if OverrideClass managed to override the original
    // method or not.
    Method method = ParentClass.class.getDeclaredMethod(methodName);
    int result1 = (int) method.invoke(new ParentClass());
    int result2 = (int) method.invoke(new OverrideClass());
    return (result1 > 0) && (result2 < 0);
  }
}

// INSTANCE FIELD GET

class LinkFieldGetWhitelist {
  public static int access() {
    return new ParentClass().fieldPublicWhitelist;
  }
}

class LinkFieldGetLightGreylist {
  public static int access() {
    return new ParentClass().fieldPublicLightGreylist;
  }
}

class LinkFieldGetDarkGreylist {
  public static int access() {
    return new ParentClass().fieldPublicDarkGreylist;
  }
}

class LinkFieldGetBlacklist {
  public static int access() {
    return new ParentClass().fieldPublicBlacklist;
  }
}

// INSTANCE FIELD SET

class LinkFieldSetWhitelist {
  public static void access(int x) {
    // Need to use a different field from the getter to bypass DexCache.
    new ParentClass().fieldPublicWhitelistB = x;
  }
}

class LinkFieldSetLightGreylist {
  public static void access(int x) {
    // Need to use a different field from the getter to bypass DexCache.
    new ParentClass().fieldPublicLightGreylistB = x;
  }
}

class LinkFieldSetDarkGreylist {
  public static void access(int x) {
    // Need to use a different field from the getter to bypass DexCache.
    new ParentClass().fieldPublicDarkGreylistB = x;
  }
}

class LinkFieldSetBlacklist {
  public static void access(int x) {
    // Need to use a different field from the getter to bypass DexCache.
    new ParentClass().fieldPublicBlacklistB = x;
  }
}

// STATIC FIELD GET

class LinkFieldGetStaticWhitelist {
  public static int access() {
    return ParentClass.fieldPublicStaticWhitelist;
  }
}

class LinkFieldGetStaticLightGreylist {
  public static int access() {
    return ParentClass.fieldPublicStaticLightGreylist;
  }
}

class LinkFieldGetStaticDarkGreylist {
  public static int access() {
    return ParentClass.fieldPublicStaticDarkGreylist;
  }
}

class LinkFieldGetStaticBlacklist {
  public static int access() {
    return ParentClass.fieldPublicStaticBlacklist;
  }
}

// STATIC FIELD SET

class LinkFieldSetStaticWhitelist {
  public static void access(int x) {
    // Need to use a different field from the getter to bypass DexCache.
    ParentClass.fieldPublicStaticWhitelistB = x;
  }
}

class LinkFieldSetStaticLightGreylist {
  public static void access(int x) {
    // Need to use a different field from the getter to bypass DexCache.
    ParentClass.fieldPublicStaticLightGreylistB = x;
  }
}

class LinkFieldSetStaticDarkGreylist {
  public static void access(int x) {
    // Need to use a different field from the getter to bypass DexCache.
    ParentClass.fieldPublicStaticDarkGreylistB = x;
  }
}

class LinkFieldSetStaticBlacklist {
  public static void access(int x) {
    // Need to use a different field from the getter to bypass DexCache.
    ParentClass.fieldPublicStaticBlacklistB = x;
  }
}

// INVOKE INSTANCE METHOD

class LinkMethodWhitelist {
  public static int access() {
    return new ParentClass().methodPublicWhitelist();
  }
}

class LinkMethodLightGreylist {
  public static int access() {
    return new ParentClass().methodPublicLightGreylist();
  }
}

class LinkMethodDarkGreylist {
  public static int access() {
    return new ParentClass().methodPublicDarkGreylist();
  }
}

class LinkMethodBlacklist {
  public static int access() {
    return new ParentClass().methodPublicBlacklist();
  }
}

// INVOKE STATIC METHOD

class LinkMethodStaticWhitelist {
  public static int access() {
    return ParentClass.methodPublicStaticWhitelist();
  }
}

class LinkMethodStaticLightGreylist {
  public static int access() {
    return ParentClass.methodPublicStaticLightGreylist();
  }
}

class LinkMethodStaticDarkGreylist {
  public static int access() {
    return ParentClass.methodPublicStaticDarkGreylist();
  }
}

class LinkMethodStaticBlacklist {
  public static int access() {
    return ParentClass.methodPublicStaticBlacklist();
  }
}
