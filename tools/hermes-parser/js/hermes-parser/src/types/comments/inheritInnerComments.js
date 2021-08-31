/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * @format
 */

import inherit from '../inherit';

export default function inheritInnerComments(
  child: Object,
  parent: Object
): void {
  inherit('innerComments', child, parent);
}
