//
//  MyImage.swift
//  MyUI
//
//  Created by Yuta Saito on 2023/06/26.
//

import Foundation
import SwiftUI

private let bundle = Bundle(for: ResourceFinder.self)

private class ResourceFinder {}

public struct MyImage: View {
    public init() {}

    public var body: some View {
        Image("my-pink.square", bundle: bundle)
        Text(bundle.description)
            .font(.caption)
    }
}

#Preview {
    MyImage()
}
